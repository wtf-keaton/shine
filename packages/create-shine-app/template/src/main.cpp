#include <shine/app.hpp>
#include <shine/command.hpp>
#include <shine/components/fs.hpp>
#include <shine/components/windows.hpp>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

#ifndef _DEBUG
#include "embedded_assets.hpp"
#include <shine_config.hpp>
#endif

SHINE_COMMAND(greet) {
    std::string name_str = args["name"];

    return nlohmann::json{{"result", name_str}};
}

int main() {
    try {
#ifndef _DEBUG
        shine::App app(shine::embedded::kConfig);
#else
        shine::App app;
#endif
#ifndef _DEBUG
        app.GetWebView().SetAssetProvider([](std::string_view relPath)
        -> std::optional<shine::engine::WebView::AssetResponse> {
                auto a = shine_app::embedded_assets::Find(relPath);
                if (!a) return std::nullopt;

                shine::engine::WebView::AssetResponse res;
                res.mime = a->mime;
                res.bytes = std::span<const std::uint8_t>(a->data, a->size);
                return res;
            });
#endif

        app.RegisterComponent<shine::components::FileSystem>();
        app.RegisterComponent<shine::components::Window>();

        app.GetRouter().AddHandlers({SHINE_HANDLER(greet)});

        return app.Run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal framework error: " << e.what() << std::endl;
        return 1;
    }
}
