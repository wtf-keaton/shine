#include <shine/app.hpp>
#include <stdexcept>

#include "shine/config.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>
#endif

namespace shine {
    class App::Impl {
    public:
        Impl(const std::string &config_path) {
#ifdef _WIN32
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to initialize COM environment");
            }
#endif

            AppConfig appConfig = AppConfig::Load(config_path);

            router_.SetAllowedCommands(appConfig.allowed_commands);

            engine::WindowConfig winConfig;
            winConfig.title = appConfig.title;
            winConfig.width = appConfig.width;
            winConfig.height = appConfig.height;
            winConfig.frameless = appConfig.frameless;
            winConfig.resizable = true;

            mainWindow_ = std::make_unique<engine::Window>(winConfig);

            mainWindow_->OnResize([this](uint32_t width, uint32_t height) {
                webView_->Resize(width, height);
            });

            mainWindow_->OnClose([]() {
                return true;
            });

            webView_ = std::make_unique<engine::WebView>(*mainWindow_);

            webView_->OnMessageReceived([this](const std::string &msg) {
                std::string js_response = router_.Route(msg);

                if (!js_response.empty()) {
                    webView_->ExecuteScript(js_response);
                }
            });
        }

        ~Impl() {
            webView_.reset();
            mainWindow_.reset();
#ifdef _WIN32
            CoUninitialize();
#endif
        }

        int Run() {
            mainWindow_->Show();
            //TODO: Move it to components
            GetRouter().AddHandler("window_drag", [&](const nlohmann::json &payload) {
                HWND hwnd = static_cast<HWND>(GetMainWindow().GetNativeHandle());


                ReleaseCapture();
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

                return nlohmann::json({{"status", "dragging"}});
            });
#ifdef _DEBUG
            GetWebView().Navigate("http://localhost:5173");
#else
            GetWebView().Navigate("http://shine.app/index.html");
#endif

#ifdef _WIN32
            MSG msg = {0};
            while (GetMessageW(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            return static_cast<int>(msg.wParam);
#else
            throw std::runtime_error("Message loop is not implemented for this OS yet.");
            return 0;
#endif
        }

        engine::Window &GetMainWindow() {
            return *mainWindow_;
        }

        engine::WebView &GetWebView() { return *webView_; }

        ipc::Router &GetRouter() { return router_; }

    private:
        std::unique_ptr<engine::Window> mainWindow_;
        std::unique_ptr<engine::WebView> webView_;
        ipc::Router router_;
    };

    App::App(const std::string &config_path) : pImpl_(std::make_unique<Impl>(config_path)) {
    }

    App::~App() = default;

    int App::Run() { return pImpl_->Run(); }
    engine::Window &App::GetMainWindow() { return pImpl_->GetMainWindow(); }
    engine::WebView &App::GetWebView() { return pImpl_->GetWebView(); }
    ipc::Router &App::GetRouter() { return pImpl_->GetRouter(); }
}
