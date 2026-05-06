#include <shine/engine/webview.hpp>
#include <shine/engine/window.hpp>

#include <Windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <wrl/implements.h>
#include <WebView2.h>
#include <stdexcept>
#include <shlwapi.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <thread>

using namespace Microsoft::WRL;

static std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int) utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int) utf8.size(), &result[0], size);
    return result;
}

static std::string WideToUtf8(const std::wstring &wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}

namespace shine::engine {
    class WebView::Impl {
    public:
        Impl(Window &window) : hwnd_(static_cast<HWND>(window.GetNativeHandle())) {
            InitializeWebView();
        }

        ~Impl() {
            if (webview_) {
                if (hasWebMessageToken_) {
                    webview_->remove_WebMessageReceived(webMessageToken_);
                    hasWebMessageToken_ = false;
                }
                if (hasResourceRequestedToken_) {
                    webview_->remove_WebResourceRequested(resourceRequestedToken_);
                    hasResourceRequestedToken_ = false;
                }
            }
            if (controller_) {
                controller_->Close();
            }
        }

        void Navigate(std::string_view url) {
            if (webview_) {
                webview_->Navigate(Utf8ToWide(url).c_str());
            } else {
                pending_url_ = url;
            }
        }

        void SetHTML(std::string_view html) {
            if (webview_) {
                webview_->NavigateToString(Utf8ToWide(html).c_str());
            } else {
                pending_html_ = html;
            }
        }

        void ExecuteScript(std::string_view script) {
            if (webview_) {
                webview_->ExecuteScript(Utf8ToWide(script).c_str(), nullptr);
            }
        }

        void PostJsonMessage(std::string_view json) {
            if (webview_) {
                webview_->PostWebMessageAsJson(Utf8ToWide(json).c_str());
            }
        }

        void PostStringMessage(std::string_view message) {
            if (webview_) {
                webview_->PostWebMessageAsString(Utf8ToWide(message).c_str());
            }
        }

        void SetAssetProvider(WebView::AssetProvider provider) {
            assetProvider_ = std::move(provider);
        }

        void Resize(uint32_t width, uint32_t height) {
            if (controller_) {
                if (width == currentWidth_ && height == currentHeight_) {
                    return;
                }

                currentWidth_ = width;
                currentHeight_ = height;

                RECT bounds = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
                controller_->put_Bounds(bounds);
            }
        }

        void ForceGarbageCollection() {
            if (!webview_) return;

            webview_->CallDevToolsProtocolMethod(
                L"Runtime.collectGarbage",
                L"{}",
                Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
                    [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; }).Get());
        }

        WebView::MessageCallback onMessage_;

    private:
        HWND hwnd_ = nullptr;
        ComPtr<ICoreWebView2Environment> env_;
        ComPtr<ICoreWebView2Controller> controller_;
        ComPtr<ICoreWebView2> webview_;
        std::string pending_url_;
        std::string pending_html_;
        WebView::AssetProvider assetProvider_;
        uint32_t currentWidth_ = 0;
        uint32_t currentHeight_ = 0;
        EventRegistrationToken webMessageToken_{};
        EventRegistrationToken resourceRequestedToken_{};
        bool hasWebMessageToken_ = false;
        bool hasResourceRequestedToken_ = false;

        void InitializeWebView() {
            auto tempDir = std::filesystem::temp_directory_path() / "Shine_WebView_Data";
            std::wstring userDataFolder = tempDir.wstring();

            CreateEnvironment(userDataFolder);
        }

        void CreateEnvironment(const std::wstring& userDataFolder) {
            const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
                nullptr,
                userDataFolder.c_str(),
                nullptr,
                Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                    [this](
                        HRESULT result,
                        ICoreWebView2Environment* env
                    ) -> HRESULT {
                        if (FAILED(result)) {
                            std::cerr << "[Shine WebView2] Failed to create environment. HRESULT: 0x"
                                      << std::hex << result << std::dec << std::endl;
                            return S_OK;
                        }

                        env_ = env;
                        CreateController();
                        return S_OK;
                    }).Get()
            );

            if (FAILED(hr)) {
                std::cerr << "[Shine WebView2] Environment creation call failed. HRESULT: 0x"
                          << std::hex << hr << std::dec << std::endl;
            }
        }

        void CreateController() {
            env_->CreateCoreWebView2Controller(
                hwnd_,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(result)) {
                            std::cerr << "[Shine WebView2] Failed to create controller. HRESULT: 0x"
                                      << std::hex << result << std::dec << std::endl;
                            return S_OK;
                        }

                        controller_ = controller;
                        controller_->get_CoreWebView2(&webview_);

                        RECT bounds;
                        GetClientRect(hwnd_, &bounds);
                        controller_->put_Bounds(bounds);

                        ApplyMemorySettings();

                        SetupIpc();
                        SetupResourceInterceptor();

                        if (!pending_url_.empty()) {
                            Navigate(pending_url_);
                            pending_url_.clear();
                        }

                        if (!pending_html_.empty()) {
                            SetHTML(pending_html_);
                            pending_html_.clear();
                        }
                        return S_OK;
                    }).Get()
            );
        }

        void ApplyMemorySettings() {
            ComPtr<ICoreWebView2Settings> settings;
            if (FAILED(webview_->get_Settings(&settings))) return;

            settings->put_IsWebMessageEnabled(TRUE);
            settings->put_AreDefaultScriptDialogsEnabled(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);
#ifndef _DEBUG
            settings->put_AreDevToolsEnabled(FALSE);
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_IsBuiltInErrorPageEnabled(FALSE);
#else
            settings->put_AreDevToolsEnabled(TRUE);
            settings->put_AreDefaultContextMenusEnabled(TRUE);
#endif

            ComPtr<ICoreWebView2Settings3> settings3;
            if (SUCCEEDED(settings.As(&settings3))) {
#ifndef _DEBUG
                settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
#endif
            }

            ComPtr<ICoreWebView2Settings4> settings4;
            if (SUCCEEDED(settings.As(&settings4))) {
                settings4->put_IsPasswordAutosaveEnabled(FALSE);
                settings4->put_IsGeneralAutofillEnabled(FALSE);
            }

            ComPtr<ICoreWebView2Settings5> settings5;
            if (SUCCEEDED(settings.As(&settings5))) {
                settings5->put_IsPinchZoomEnabled(FALSE);
            }

            ComPtr<ICoreWebView2Settings6> settings6;
            if (SUCCEEDED(settings.As(&settings6))) {
                settings6->put_IsSwipeNavigationEnabled(FALSE);
            }
        }

        void SetupIpc() {
            if (hasWebMessageToken_) return;

            if (SUCCEEDED(webview_->add_WebMessageReceived(
                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                    [this](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                        LPWSTR messageRaw;
                        if (SUCCEEDED(args->get_WebMessageAsJson(&messageRaw))) {
                            std::wstring wmsg(messageRaw);
                            CoTaskMemFree(messageRaw);

                            if (onMessage_) {
                                onMessage_(WideToUtf8(wmsg));
                            }
                        }
                        return S_OK;
                    }).Get(), &webMessageToken_))) {
                hasWebMessageToken_ = true;
            }
        }

        void SetupResourceInterceptor() {
            if (hasResourceRequestedToken_) return;

            if (FAILED(webview_->AddWebResourceRequestedFilter(
                    L"http://shine-ui.app/*",
                    COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL))) {
                return;
            }

            if (SUCCEEDED(webview_->add_WebResourceRequested(
                Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                    [this](ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
                        ComPtr<ICoreWebView2WebResourceRequest> request;
                        args->get_Request(&request);
                        LPWSTR uriRaw;
                        request->get_Uri(&uriRaw);
                        std::wstring uri(uriRaw);
                        CoTaskMemFree(uriRaw);

                        if (uri.find(L"http://shine-ui.app/") == 0) {
                            return HandleLocalResource(uri, args);
                        }
                        return S_OK;
                    }).Get(), &resourceRequestedToken_))) {
                hasResourceRequestedToken_ = true;
            }
        }

        HRESULT HandleLocalResource(const std::wstring &uri, ICoreWebView2WebResourceRequestedEventArgs *args) const {
            const std::wstring prefix = L"http://shine-ui.app/";

            if (uri.find(prefix) != 0) return S_OK;

            std::wstring relativePath = uri.substr(prefix.length());

            const std::size_t pathEnd = relativePath.find_first_of(L"?#");
            if (pathEnd != std::wstring::npos) {
                relativePath.erase(pathEnd);
            }

            if (relativePath.empty() || relativePath == L"/") relativePath = L"index.html";
            if (relativePath[0] == L'/') relativePath.erase(0, 1);

            if (assetProvider_) {
                std::string relUtf8 = WideToUtf8(relativePath);
                std::optional<WebView::AssetResponse> asset = assetProvider_(relUtf8);
                if (asset && !asset->bytes.empty()) {
                    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, asset->bytes.size());
                    if (!hmem) return S_OK;

                    void *dst = GlobalLock(hmem);
                    if (!dst) {
                        GlobalFree(hmem);
                        return S_OK;
                    }
                    memcpy(dst, asset->bytes.data(), asset->bytes.size());
                    GlobalUnlock(hmem);

                    ComPtr<IStream> stream;
                    HRESULT hr = CreateStreamOnHGlobal(hmem, TRUE, &stream);
                    if (FAILED(hr) || !stream) {
                        GlobalFree(hmem);
                        return S_OK;
                    }

                    std::wstring mimeW = Utf8ToWide(asset->mime.empty() ? "application/octet-stream" : asset->mime);
                    std::wstring headers = L"Content-Type: " + mimeW;
                    if (relativePath != L"index.html") {
                        headers += L"\r\nCache-Control: public, max-age=31536000, immutable";
                    } else {
                        headers += L"\r\nCache-Control: no-cache";
                    }

                    ComPtr<ICoreWebView2WebResourceResponse> response;
                    env_->CreateWebResourceResponse(
                        stream.Get(),
                        200,
                        L"OK",
                        headers.c_str(),
                        &response
                    );
                    args->put_Response(response.Get());
                    return S_OK;
                }
            }

            auto assetsPath = std::filesystem::current_path() / "assets" / relativePath;

            if (!std::filesystem::exists(assetsPath)) {
                std::wcerr << L"[Shine Error] File not found: " << assetsPath.wstring() << std::endl;
                return S_OK;
            }
            IStream *postDataStream = nullptr;
            HRESULT hr = SHCreateStreamOnFileEx(assetsPath.c_str(), STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr,
                                                &postDataStream);

            if (SUCCEEDED(hr)) {
                const wchar_t *mimeType = L"text/html";
                auto ext = assetsPath.extension().wstring();
                if (ext == L".css") mimeType = L"text/css";
                else if (ext == L".js") mimeType = L"application/javascript";
                else if (ext == L".png") mimeType = L"image/png";
                else if (ext == L".svg") mimeType = L"image/svg+xml";

                std::wstring headers = L"Content-Type: " + std::wstring(mimeType);
                if (relativePath != L"index.html") {
                    headers += L"\r\nCache-Control: public, max-age=31536000, immutable";
                } else {
                    headers += L"\r\nCache-Control: no-cache";
                }

                ComPtr<ICoreWebView2WebResourceResponse> response;
                env_->CreateWebResourceResponse(postDataStream, 200, L"OK", headers.c_str(), &response);
                args->put_Response(response.Get());
            }

            return S_OK;
        }
    };

    WebView::WebView(Window &window) : pImpl_(std::make_unique<Impl>(window)) {
    }

    WebView::~WebView() = default;

    WebView::WebView(WebView &&) noexcept = default;

    WebView &WebView::operator=(WebView &&) noexcept = default;

    void WebView::Navigate(std::string_view url) { pImpl_->Navigate(url); }
    void WebView::SetHTML(std::string_view html) { pImpl_->SetHTML(html); }
    void WebView::ExecuteScript(std::string_view js) { pImpl_->ExecuteScript(js); }
    void WebView::PostJsonMessage(std::string_view json) { pImpl_->PostJsonMessage(json); }
    void WebView::PostStringMessage(std::string_view message) { pImpl_->PostStringMessage(message); }
    void WebView::SetAssetProvider(AssetProvider provider) { pImpl_->SetAssetProvider(std::move(provider)); }
    void WebView::OnMessageReceived(MessageCallback callback) { pImpl_->onMessage_ = std::move(callback); }
    void WebView::Resize(uint32_t width, uint32_t height) { pImpl_->Resize(width, height); }
    void WebView::CollectGarbage() { pImpl_->ForceGarbageCollection(); }
}
