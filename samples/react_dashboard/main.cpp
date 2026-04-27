#include <shine/app.hpp>
#include <shine/components/fs.hpp>
#include <iostream>
#include <Windows.h>
#include <filesystem>
#include <fstream>

#include "embedded_assets.hpp"

static std::filesystem::path MakeTempDir() {
    auto base = std::filesystem::temp_directory_path() / "Shine_Sample_ReactDashboard";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);

    // Best-effort unique folder (avoids collisions across runs).
    auto unique = base / std::to_string(GetCurrentProcessId());
    std::filesystem::create_directories(unique, ec);
    return unique;
}

static void WriteFileBytes(const std::filesystem::path& file, const std::uint8_t* data, std::size_t size) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);

    std::ofstream out(file, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to write asset file: " + file.string());
    }
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

static std::filesystem::path ExtractEmbeddedAssetsToTemp() {
    auto root = MakeTempDir();
    auto assetsRoot = root / "assets";
    std::error_code ec;
    std::filesystem::create_directories(assetsRoot, ec);

    for (std::size_t i = 0; i < shine_sample::embedded_assets::kAssetsCount; i++) {
        const auto& a = shine_sample::embedded_assets::kAssets[i];
        auto outPath = assetsRoot / a.path;
        WriteFileBytes(outPath, a.data, a.size);
    }

    return root;
}

int main() {
    std::cout << "Starting Shine Application" << std::endl;

    try {
        // Shine's current WebView implementation serves files from:
        //   std::filesystem::current_path() / "assets" / <relativePath>
        //
        // We can't change the library here (per sample constraints), so the sample
        // embeds the frontend build into the binary and extracts it to a temp folder.
        auto tempRoot = ExtractEmbeddedAssetsToTemp();
        std::filesystem::current_path(tempRoot);

        shine::App app;

        app.RegisterComponent<shine::components::FileSystem>();

        return app.Run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal framework error: " << e.what() << std::endl;
        return 1;
    }
}
