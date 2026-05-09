#include <shine/components/windows.hpp>
#include <shine/app.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shine::components {
    void Window::Init(App &app) {
        app.GetRouter().AddProtectedHandler("window_drag", [&](const nlohmann::json &payload) {
            const auto hwnd = static_cast<HWND>(app.GetMainWindow().GetNativeHandle());
            POINT cursor{};
            GetCursorPos(&cursor);

            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(cursor.x, cursor.y));

            return nlohmann::json({{"status", "dragging"}});
        });

        app.GetRouter().AddProtectedHandler("close", [&](const nlohmann::json &payload) {
            app.GetMainWindow().Close();

            return nlohmann::json({{"status", "closed"}});
        });

        app.GetRouter().AddProtectedHandler("minimize", [&](const nlohmann::json &payload) {
            const auto hwnd = static_cast<HWND>(app.GetMainWindow().GetNativeHandle());
            SendMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);

            return nlohmann::json({{"status", "minimized"}});
        });

        app.GetRouter().AddProtectedHandler("maximize", [&](const nlohmann::json &payload) {
            const auto hwnd = static_cast<HWND>(app.GetMainWindow().GetNativeHandle());
            const bool is_maximized = IsZoomed(hwnd);
            SendMessageW(hwnd, WM_SYSCOMMAND, is_maximized ? SC_RESTORE : SC_MAXIMIZE, 0);

            return nlohmann::json({{"status", is_maximized ? "restored" : "maximized"}});
        });

        app.GetRouter().AddPermissions({
            {"window:default", {"window_drag", "close", "minimize", "maximize"}},
            {"window:allow-drag", {"window_drag"}},
            {"window:allow-close", {"close"}},
            {"window:allow-minimize", {"minimize"}},
            {"window:allow-maximize", {"maximize"}}
        });
    };
}
