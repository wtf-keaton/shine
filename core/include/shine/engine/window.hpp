#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <cstdint>

namespace shine::engine {
    struct WindowConfig {
        std::string title = "Shine App";
        uint32_t width = 800;
        uint32_t height = 600;
        bool resizable = true;
        bool frameless = false;
    };

    class Window final {
    public:
        explicit Window(const WindowConfig& config);

        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        Window(Window&&) noexcept;
        Window& operator=(Window&&) noexcept;

        void Show();
        void Hide();
        void Close();

        void SetTitle(std::string_view title);

        [[nodiscard]] uint32_t GetWidth() const;
        [[nodiscard]] uint32_t GetHeight() const;

        void SetSize(uint32_t width, uint32_t height);

        using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
        using CloseCallback = std::function<bool()>;

        void OnResize(ResizeCallback callback);
        void OnClose(CloseCallback callback);

        [[nodiscard]] void* GetNativeHandle() const;

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };
}