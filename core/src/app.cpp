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
        Impl(const std::string &config) {
#ifdef _WIN32
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to initialize COM environment");
            }
#endif

            AppConfig appConfig = AppConfig::Load(config);

            if (appConfig.has_capabilities_policy) {
                router_.SetAllowedPermissions(appConfig.allowed_permissions);
                if (!appConfig.allowed_commands.empty()) {
                    router_.SetAllowedCommands(appConfig.allowed_commands);
                }
            }

            engine::WindowConfig winConfig;
            winConfig.title = appConfig.title;
            winConfig.width = appConfig.width;
            winConfig.height = appConfig.height;
            winConfig.frameless = appConfig.frameless;
            winConfig.resizable = appConfig.resizable;
            winConfig.centered = appConfig.centered;

            mainWindow_ = std::make_unique<engine::Window>(winConfig);
            webView_ = std::make_unique<engine::WebView>(*mainWindow_);

            mainWindow_->OnResize([this](const uint32_t width, const uint32_t height) {
                if (webView_) {
                    webView_->Resize(width, height);
                }
            });

            mainWindow_->OnClose([] {
                return true;
            });

            webView_->OnMessageReceived([this](const std::string &msg) {
                const auto response = router_.Route(msg);
                const std::string response_json = response.dump();
                webView_->PostJsonMessage(response_json);
                webView_->PostStringMessage(response_json);
#ifdef _DEBUG
                webView_->ExecuteScript(
                    "window.__SHINE_IPC_RECEIVE__ && window.__SHINE_IPC_RECEIVE__("
                    + response_json +
                    ");");
#endif
            });
        }

        ~Impl() {
            webView_.reset();
            mainWindow_.reset();
#ifdef _WIN32
            CoUninitialize();
#endif
        }

        int Run() const {
            mainWindow_->Show();
#ifdef _DEBUG
            GetWebView().Navigate("http://localhost:1745");
#else
            GetWebView().Navigate("http://shine-ui.app/");
#endif

#ifdef _WIN32
            MSG msg = {nullptr};
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

        [[nodiscard]] engine::Window &GetMainWindow() const {
            return *mainWindow_;
        }

        [[nodiscard]] engine::WebView &GetWebView() const { return *webView_; }

        ipc::Router &GetRouter() { return router_; }

    private:
        std::unique_ptr<engine::Window> mainWindow_;
        std::unique_ptr<engine::WebView> webView_;
        ipc::Router router_;
    };

    App::App(const std::string &config_path) : pImpl_(std::make_unique<Impl>(config_path)) {
    }

    App::~App() = default;
    App::App(App&&) noexcept = default;
    App& App::operator=(App&&) noexcept = default;

    int App::Run() const { return pImpl_->Run(); }
    engine::Window &App::GetMainWindow() const { return pImpl_->GetMainWindow(); }
    engine::WebView &App::GetWebView() const { return pImpl_->GetWebView(); }
    ipc::Router &App::GetRouter() const { return pImpl_->GetRouter(); }
}
