#include <shine/engine/window.hpp>
#include <Windows.h>
#include <windowsx.h>
#include <array>
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
            UnregisterClassW(resizeGripClassName_.c_str(), GetModuleHandle(nullptr));
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
        enum class ResizeGrip : size_t {
            Left,
            Right,
            Top,
            Bottom,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight,
            Count
        };

        HWND hwnd_ = nullptr;
        WindowConfig config_;
        std::wstring className_ = L"ShineWindowClass";
        std::wstring resizeGripClassName_ = L"ShineResizeGripClass";
        std::array<HWND, static_cast<size_t>(ResizeGrip::Count)> resizeGrips_{};

        static LPCWSTR ResizeGripHitTestProperty() {
            return L"ShineResizeGripHitTest";
        }

        [[nodiscard]] int GetResizeBorderThickness() const {
            return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        }

        [[nodiscard]] LRESULT HitTestFramelessResize(HWND hwnd, LPARAM lParam) const {
            if (!config_.frameless || !config_.resizable || IsZoomed(hwnd)) {
                return HTCLIENT;
            }

            RECT rect{};
            if (!GetWindowRect(hwnd, &rect)) {
                return HTCLIENT;
            }

            const POINT cursor{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };
            const int border = GetResizeBorderThickness();

            const bool left = cursor.x >= rect.left && cursor.x < rect.left + border;
            const bool right = cursor.x < rect.right && cursor.x >= rect.right - border;
            const bool top = cursor.y >= rect.top && cursor.y < rect.top + border;
            const bool bottom = cursor.y < rect.bottom && cursor.y >= rect.bottom - border;

            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;

            return HTCLIENT;
        }

        [[nodiscard]] static int GripHitTest(ResizeGrip grip) {
            switch (grip) {
                case ResizeGrip::Left: return HTLEFT;
                case ResizeGrip::Right: return HTRIGHT;
                case ResizeGrip::Top: return HTTOP;
                case ResizeGrip::Bottom: return HTBOTTOM;
                case ResizeGrip::TopLeft: return HTTOPLEFT;
                case ResizeGrip::TopRight: return HTTOPRIGHT;
                case ResizeGrip::BottomLeft: return HTBOTTOMLEFT;
                case ResizeGrip::BottomRight: return HTBOTTOMRIGHT;
                case ResizeGrip::Count: break;
            }

            return HTCLIENT;
        }

        [[nodiscard]] static LPCWSTR GripCursor(int hitTest) {
            switch (hitTest) {
                case HTLEFT:
                case HTRIGHT:
                    return MAKEINTRESOURCEW(32644);
                case HTTOP:
                case HTBOTTOM:
                    return MAKEINTRESOURCEW(32645);
                case HTTOPLEFT:
                case HTBOTTOMRIGHT:
                    return MAKEINTRESOURCEW(32642);
                case HTTOPRIGHT:
                case HTBOTTOMLEFT:
                    return MAKEINTRESOURCEW(32643);
                default:
                    return MAKEINTRESOURCEW(32512);
            }
        }

        void RegisterResizeGripClass() const {
            WNDCLASSEXW wc;
            ZeroMemory(&wc, sizeof(WNDCLASSEXW));

            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.lpfnWndProc = Impl::ResizeGripWndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
            wc.hbrBackground = nullptr;
            wc.lpszClassName = resizeGripClassName_.c_str();

            if (!RegisterClassExW(&wc)) {
                DWORD err = GetLastError();
                if (err != ERROR_CLASS_ALREADY_EXISTS) {
                    throw std::runtime_error(
                        "Failed to register Win32 resize grip class. System Error Code: " + std::to_string(err));
                }
            }
        }

        void CreateResizeGrips() {
            if (!config_.frameless || !config_.resizable) {
                return;
            }

            RegisterResizeGripClass();

            for (size_t i = 0; i < resizeGrips_.size(); ++i) {
                const auto grip = static_cast<ResizeGrip>(i);
                const int hitTest = GripHitTest(grip);

                HWND gripHwnd = CreateWindowExW(
                    WS_EX_TRANSPARENT,
                    resizeGripClassName_.c_str(),
                    L"",
                    WS_CHILD | WS_VISIBLE,
                    0,
                    0,
                    1,
                    1,
                    hwnd_,
                    nullptr,
                    GetModuleHandleW(nullptr),
                    nullptr
                );

                if (gripHwnd) {
                    SetPropW(gripHwnd, ResizeGripHitTestProperty(), LongToHandle(hitTest));
                    resizeGrips_[i] = gripHwnd;
                }
            }

            PositionResizeGrips();
        }

        void PositionResizeGrips() const {
            if (!config_.frameless || !config_.resizable || IsZoomed(hwnd_)) {
                for (HWND grip : resizeGrips_) {
                    if (grip) ShowWindow(grip, SW_HIDE);
                }
                return;
            }

            RECT rect{};
            GetClientRect(hwnd_, &rect);

            const int width = rect.right - rect.left;
            const int height = rect.bottom - rect.top;
            const int border = GetResizeBorderThickness();
            const int corner = border * 2;

            struct GripBounds {
                int x;
                int y;
                int width;
                int height;
            };

            const std::array<GripBounds, static_cast<size_t>(ResizeGrip::Count)> bounds{{
                {0, corner, border, height - corner * 2},
                {width - border, corner, border, height - corner * 2},
                {corner, 0, width - corner * 2, border},
                {corner, height - border, width - corner * 2, border},
                {0, 0, corner, corner},
                {width - corner, 0, corner, corner},
                {0, height - corner, corner, corner},
                {width - corner, height - corner, corner, corner}
            }};

            for (size_t i = 0; i < resizeGrips_.size(); ++i) {
                HWND grip = resizeGrips_[i];
                if (!grip) continue;

                const auto& gripBounds = bounds[i];
                const bool visible = gripBounds.width > 0 && gripBounds.height > 0;
                ShowWindow(grip, visible ? SW_SHOWNA : SW_HIDE);

                if (visible) {
                    SetWindowPos(
                        grip,
                        HWND_TOP,
                        gripBounds.x,
                        gripBounds.y,
                        gripBounds.width,
                        gripBounds.height,
                        SWP_NOACTIVATE
                    );
                }
            }
        }

        [[nodiscard]] LRESULT RemoveFramelessNonClientArea(HWND hwnd, WPARAM wParam, LPARAM lParam) const {
            if (!config_.frameless) {
                return DefWindowProcW(hwnd, WM_NCCALCSIZE, wParam, lParam);
            }

            if (wParam == TRUE && IsZoomed(hwnd)) {
                auto params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);
                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(MONITORINFO);

                if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
                    params->rgrc[0] = monitorInfo.rcWork;
                }
            }

            return 0;
        }

        void ApplyFramelessMaximizedBounds(HWND hwnd, LPARAM lParam) const {
            if (!config_.frameless) {
                return;
            }

            auto minMaxInfo = reinterpret_cast<MINMAXINFO *>(lParam);
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(MONITORINFO);

            if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
                return;
            }

            const RECT& monitor = monitorInfo.rcMonitor;
            const RECT& workArea = monitorInfo.rcWork;

            minMaxInfo->ptMaxPosition.x = workArea.left - monitor.left;
            minMaxInfo->ptMaxPosition.y = workArea.top - monitor.top;
            minMaxInfo->ptMaxSize.x = workArea.right - workArea.left;
            minMaxInfo->ptMaxSize.y = workArea.bottom - workArea.top;
            minMaxInfo->ptMaxTrackSize.x = minMaxInfo->ptMaxSize.x;
            minMaxInfo->ptMaxTrackSize.y = minMaxInfo->ptMaxSize.y;
        }

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
            DWORD style = WS_OVERLAPPEDWINDOW;

            if (!config_.resizable) {
                style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
            }

            RECT windowRect = {
                0,
                0,
                static_cast<LONG>(config_.width),
                static_cast<LONG>(config_.height)
            };

            if (!config_.frameless) {
                AdjustWindowRectEx(&windowRect, style, FALSE, 0);
            }

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

            if (config_.frameless) {
                SetWindowPos(
                    hwnd_,
                    nullptr,
                    0,
                    0,
                    0,
                    0,
                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                );
            }

            CreateResizeGrips();
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

        static LRESULT CALLBACK ResizeGripWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            const int hitTest = HandleToLong(GetPropW(hwnd, ResizeGripHitTestProperty()));

            switch (msg) {
                case WM_NCHITTEST:
                    return HTCLIENT;
                case WM_SETCURSOR:
                    SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(GripCursor(hitTest))));
                    return TRUE;
                case WM_LBUTTONDOWN: {
                    HWND root = GetAncestor(hwnd, GA_ROOT);
                    POINT cursor{};
                    GetCursorPos(&cursor);

                    ReleaseCapture();
                    SendMessageW(root, WM_NCLBUTTONDOWN, hitTest, MAKELPARAM(cursor.x, cursor.y));
                    return 0;
                }
                case WM_ERASEBKGND:
                    return TRUE;
                case WM_PAINT:
                    ValidateRect(hwnd, nullptr);
                    return 0;
                case WM_NCDESTROY:
                    RemovePropW(hwnd, ResizeGripHitTestProperty());
                    break;
                default:
                    break;
            }

            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) const {
            switch (msg) {
                case WM_GETMINMAXINFO: {
                    if (config_.frameless) {
                        ApplyFramelessMaximizedBounds(hwnd, lParam);
                        return 0;
                    }
                    break;
                }
                case WM_NCCALCSIZE: {
                    return RemoveFramelessNonClientArea(hwnd, wParam, lParam);
                }
                case WM_NCHITTEST: {
                    if (!config_.frameless) {
                        break;
                    }

                    const LRESULT defaultHit = DefWindowProcW(hwnd, msg, wParam, lParam);
                    if (defaultHit != HTCLIENT && defaultHit != HTNOWHERE) {
                        return defaultHit;
                    }

                    const LRESULT hit = HitTestFramelessResize(hwnd, lParam);
                    if (hit != HTCLIENT) {
                        return hit;
                    }
                    return HTCLIENT;
                }
                case WM_PARENTNOTIFY: {
                    if (LOWORD(wParam) == WM_CREATE) {
                        PositionResizeGrips();
                    }
                    break;
                }
                case WM_SIZE: {
                    if (onResize_) {
                        const uint32_t width = LOWORD(lParam);
                        const uint32_t height = HIWORD(lParam);
                        onResize_(width, height);
                    }

                    PositionResizeGrips();
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
