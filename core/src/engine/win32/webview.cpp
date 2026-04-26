#include <shine/engine/webview.hpp>
#include <shine/engine/window.hpp>

#include <Windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <stdexcept>
#include <shlwapi.h>
#include <filesystem>
#include <iostream>

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
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

namespace shine::engine {
    class WebView::Impl {
    public:
        Impl(Window &window) : hwnd_(static_cast<HWND>(window.GetNativeHandle())) {
            InitializeWebView();
        }

        ~Impl() {
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
                // Запоминаем HTML, если браузер еще не готов
                pending_html_ = html;
            }
        }

        void ExecuteScript(std::string_view script) {
            if (webview_) {
                webview_->ExecuteScript(Utf8ToWide(script).c_str(), nullptr);
            }
        }

        void Resize(uint32_t width, uint32_t height) {
            if (controller_) {
                RECT bounds = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
                controller_->put_Bounds(bounds);
            }
        }

        WebView::MessageCallback onMessage_;

    private:
        HWND hwnd_ = nullptr;
        ComPtr<ICoreWebView2Environment> env_;
        ComPtr<ICoreWebView2Controller> controller_;
        ComPtr<ICoreWebView2> webview_;
        std::string pending_url_;
        std::string pending_html_;

        void InitializeWebView() {
            auto tempDir = std::filesystem::temp_directory_path() / "Shine_WebView_Data";
            std::wstring userDataFolder = tempDir.wstring();

            CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataFolder.c_str(), nullptr,
                                                     Callback<
                                                         ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                                                         [this](HRESULT result,
                                                                ICoreWebView2Environment *env) -> HRESULT {
                                                             if (FAILED(result)) return result;

                                                             env_ = env;

                                                             env->CreateCoreWebView2Controller(hwnd_,
                                                                 Callback<
                                                                     ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                                                     [this](HRESULT result,
                                                                            ICoreWebView2Controller *controller) ->
                                                                 HRESULT {
                                                                         if (FAILED(result)) return result;

                                                                         controller_ = controller;
                                                                         controller_->get_CoreWebView2(&webview_);

                                                                         RECT bounds;
                                                                         GetClientRect(hwnd_, &bounds);
                                                                         controller_->put_Bounds(bounds);

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
                                                                     }).Get());
                                                             return S_OK;
                                                         }).Get());
        }

        void SetupIpc() {
            EventRegistrationToken token;
            webview_->add_WebMessageReceived(
                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                    [this](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                        LPWSTR messageRaw;
                        if (SUCCEEDED(args->get_WebMessageAsJson(&messageRaw))) {
                            std::wstring wmsg(messageRaw);
                            CoTaskMemFree(messageRaw);

                            if (onMessage_) {
                                onMessage_(WideToUtf8(wmsg));
                            }
                        }
                        return S_OK;
                    }).Get(), &token);
        }

        void SetupResourceInterceptor() {
            webview_->AddWebResourceRequestedFilter(L"http://shine.app/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

            EventRegistrationToken token;
            webview_->add_WebResourceRequested(
                Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                    [this](ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
                        ComPtr<ICoreWebView2WebResourceRequest> request;
                        args->get_Request(&request);
                        LPWSTR uriRaw;
                        request->get_Uri(&uriRaw);
                        std::wstring uri(uriRaw);
                        CoTaskMemFree(uriRaw);

                        if (uri.find(L"http://shine.app/") == 0) {
                            return HandleLocalResource(uri, args);
                        }
                        return S_OK;
                    }).Get(), &token);
        }

        HRESULT HandleLocalResource(const std::wstring &uri, ICoreWebView2WebResourceRequestedEventArgs *args) {
            std::wstring relativePath = uri.substr(17);

            if (relativePath.empty() || relativePath == L"/") relativePath = L"index.html";
            if (relativePath[0] == L'/') relativePath.erase(0, 1);

            auto assetsPath = std::filesystem::current_path() / "assets" / relativePath;

            if (!std::filesystem::exists(assetsPath)) {
                // Вы увидите в консоли CLion, где именно программа ищет файл
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

                ComPtr<ICoreWebView2Environment> env;

                ComPtr<ICoreWebView2WebResourceResponse> response;
                env_->CreateWebResourceResponse(postDataStream, 200, L"OK",
                                                (L"Content-Type: " + std::wstring(mimeType)).c_str(), &response);
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
    void WebView::OnMessageReceived(MessageCallback callback) { pImpl_->onMessage_ = std::move(callback); }
    void WebView::Resize(uint32_t width, uint32_t height) { pImpl_->Resize(width, height); }
}
