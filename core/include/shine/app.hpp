#pragma once

#include <string>
#include <memory>
#include <vector>
#include <shine/engine/window.hpp>
#include <shine/engine/webview.hpp>
#include <shine/ipc.hpp>
#include <shine/component.hpp>


namespace shine {
    class App final {
    public:
        explicit App(const std::string& config_path = "shine.conf.json");

        ~App();

        App(const App&) = delete;
        App& operator=(const App&) = delete;

        int Run();

        engine::Window& GetMainWindow();
        engine::WebView& GetWebView();
        ipc::Router& GetRouter();

        template<typename T, typename... Args>
        void RegisterComponent(Args&&... args) {
            auto component = std::make_unique<T>(std::forward<Args>(args)...);
            component->Init(*this);
            components_.push_back(std::move(component));
        }
    private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
        std::vector<std::unique_ptr<Component>> components_;
    };
}
