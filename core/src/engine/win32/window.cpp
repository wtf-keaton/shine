#include <shine/engine/window.hpp>
#include <Windows.h>
#include <stdexcept>

static std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int) utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int) utf8.size(), &result[0], size);
    return result;
}

namespace shine::engine {
    class Window::Impl {
    public:
        Impl(const WindowConfig &config) : config_(config) {
            RegisterWindowClass();
            CreateWindowInstance();
        }

        ~Impl() {
            if (hwnd_) {
                DestroyWindow(hwnd_);
            }
            UnregisterClassW(className_.c_str(), GetModuleHandle(nullptr));
        }

        void Show() const {
            ShowWindow(hwnd_, SW_SHOW);
            UpdateWindow(hwnd_);
        }

        void Hide() const { ShowWindow(hwnd_, SW_HIDE); }

        void Close() const {
            PostMessage(hwnd_, WM_CLOSE, 0, 0);
        }

        void SetTitle(const std::string_view title) const {
            SetWindowTextW(hwnd_, Utf8ToWide(title).c_str());
        }

        void SetSize(uint32_t width, uint32_t height) const {
            SetWindowPos(
                hwnd_,
                nullptr,
                0,
                0,
                static_cast<int>(width),
                static_cast<int>(height),
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
            );
        }

        [[nodiscard]] uint32_t GetWidth() const {
            RECT rect;
            GetClientRect(hwnd_, &rect);
            return rect.right - rect.left;
        }

        [[nodiscard]] uint32_t GetHeight() const {
            RECT rect;
            GetClientRect(hwnd_, &rect);
            return rect.bottom - rect.top;
        }

        [[nodiscard]] void *GetNativeHandle() const { return hwnd_; }

        ResizeCallback onResize_;
        CloseCallback onClose_;

    private:
        HWND hwnd_ = nullptr;
        WindowConfig config_;
        std::wstring className_ = L"ShineWindowClass";

        void RegisterWindowClass() const {
            WNDCLASSEXW wc;
            ZeroMemory(&wc, sizeof(WNDCLASSEXW));

            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = Impl::WndProcSetup;
            wc.cbClsExtra = 0;
            wc.cbWndExtra = 0;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
            wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
            wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
            wc.lpszMenuName = nullptr;
            wc.lpszClassName = className_.c_str();
            wc.hIconSm = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));

            if (!RegisterClassExW(&wc)) {
                DWORD err = GetLastError();
                if (err != ERROR_CLASS_ALREADY_EXISTS) {
                    throw std::runtime_error(
                        "Failed to register Win32 window class. System Error Code: " + std::to_string(err));
                }
            }
        }

        void CreateWindowInstance() {
            DWORD style = config_.frameless ? WS_POPUP : WS_OVERLAPPEDWINDOW;

            if (config_.frameless) {
                style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
            }

            if (!config_.resizable) {
                style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
            }

            RECT windowRect = {
                0,
                0,
                static_cast<LONG>(config_.width),
                static_cast<LONG>(config_.height)
            };
            AdjustWindowRectEx(&windowRect, style, FALSE, 0);

            const int windowWidth = windowRect.right - windowRect.left;
            const int windowHeight = windowRect.bottom - windowRect.top;
            int x = CW_USEDEFAULT;
            int y = CW_USEDEFAULT;

            if (config_.centered) {
                POINT cursor{};
                GetCursorPos(&cursor);
                HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(MONITORINFO);

                if (GetMonitorInfoW(monitor, &monitorInfo)) {
                    const RECT& workArea = monitorInfo.rcWork;
                    x = workArea.left + ((workArea.right - workArea.left) - windowWidth) / 2;
                    y = workArea.top + ((workArea.bottom - workArea.top) - windowHeight) / 2;
                }
            }

            hwnd_ = CreateWindowExW(
                0, className_.c_str(), Utf8ToWide(config_.title).c_str(),
                style,
                x, y, windowWidth, windowHeight,
                nullptr, nullptr, GetModuleHandle(nullptr),
                this
            );

            if (!hwnd_) {
                throw std::runtime_error("Failed to create Win32 window");
            }
        }

        static LRESULT CALLBACK WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            if (msg == WM_NCCREATE) {
                auto pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
                auto pImpl = static_cast<Impl *>(pCreate->lpCreateParams);

                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pImpl));
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Impl::WndProcThunk));

                return pImpl->HandleMessage(hwnd, msg, wParam, lParam);
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            auto pImpl = reinterpret_cast<Impl *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (pImpl) {
                return pImpl->HandleMessage(hwnd, msg, wParam, lParam);
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) const {
            switch (msg) {
                case WM_SIZE: {
                    if (onResize_) {
                        const uint32_t width = LOWORD(lParam);
                        const uint32_t height = HIWORD(lParam);
                        onResize_(width, height);
                    }
                    return 0;
                }
                case WM_CLOSE: {
                    if (onClose_ && !onClose_()) {
                        return 0;
                    }
                    DestroyWindow(hwnd);
                    return 0;
                }
                case WM_DESTROY: {
                    PostQuitMessage(0);
                    return 0;
                }
                default: break;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    };

    Window::Window(const WindowConfig &config) : pImpl_(std::make_unique<Impl>(config)) {
    }

    Window::~Window() = default;

    Window::Window(Window &&) noexcept = default;

    Window &Window::operator=(Window &&) noexcept = default;

    void Window::Show() { pImpl_->Show(); }
    void Window::Hide() { pImpl_->Hide(); }
    void Window::Close() { pImpl_->Close(); }
    void Window::SetTitle(std::string_view title) { pImpl_->SetTitle(title); }
    uint32_t Window::GetWidth() const { return pImpl_->GetWidth(); }
    uint32_t Window::GetHeight() const { return pImpl_->GetHeight(); }
    void Window::SetSize(uint32_t width, uint32_t height) { pImpl_->SetSize(width, height); }
    void Window::OnResize(ResizeCallback callback) { pImpl_->onResize_ = std::move(callback); }
    void Window::OnClose(CloseCallback callback) { pImpl_->onClose_ = std::move(callback); }
    void *Window::GetNativeHandle() const { return pImpl_->GetNativeHandle(); }
}
