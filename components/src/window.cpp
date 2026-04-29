#include <shine/components/windows.hpp>
#include <shine/app.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shine::components {
    void Window::Init(App &app) {
        app.GetRouter().AddHandler("window_drag", [&](const nlohmann::json &payload) {
            HWND hwnd = static_cast<HWND>(app.GetMainWindow().GetNativeHandle());


            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

            return nlohmann::json({{"status", "dragging"}});
        });
    };
}
