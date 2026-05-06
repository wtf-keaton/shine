#include <shine/components/windows.hpp>
#include <shine/app.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shine::components {
    void Window::Init(App &app) {
        app.GetRouter().AddProtectedHandler("window_drag", [&](const nlohmann::json &payload) {
            const auto hwnd = static_cast<HWND>(app.GetMainWindow().GetNativeHandle());

            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

            return nlohmann::json({{"status", "dragging"}});
        });

        app.GetRouter().AddProtectedHandler("close", [&](const nlohmann::json &payload) {
            app.GetMainWindow().Close();

            return nlohmann::json({{"status", "closed"}});
        });

        app.GetRouter().AddPermissions({
            {"window:default", {"window_drag", "close"}},
            {"window:allow-drag", {"window_drag"}},
            {"window:allow-close", {"close"}}
        });
    };
}
