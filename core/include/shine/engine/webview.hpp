#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>

namespace shine::engine {
    class Window;

    class WebView final {
    public:
        explicit WebView(Window& window);
        ~WebView();

        WebView(const WebView&) = delete;
        WebView& operator=(const WebView&) = delete;
        WebView(WebView&&) noexcept;
        WebView& operator=(WebView&&) noexcept;

        void Navigate(std::string_view url);
        void SetHTML(std::string_view html);

        void ExecuteScript(std::string_view script);

        using MessageCallback = std::function<void(std::string)>;
        void OnMessageReceived(MessageCallback callback);

        void Resize(uint32_t width, uint32_t height);

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };
}