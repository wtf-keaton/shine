#include <shine/app.hpp>
#include <shine/components/fs.hpp>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

#include "embedded_assets.hpp"

int main() {
    std::cout << "Starting Shine Application" << std::endl;

    try {
        shine::App app;

#ifndef _DEBUG
        // In production we serve frontend assets from memory (no disk I/O).
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
        return app.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal framework error: " << e.what() << std::endl;
        return 1;
    }
}