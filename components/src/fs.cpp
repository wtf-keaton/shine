#include <shine/components/fs.hpp>
#include <shine/app.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace shine::components {
    void FileSystem::Init(App &app) {
        app.GetRouter().AddHandler("fs_read_text_file", [](const nlohmann::json &payload) {
            if (!payload.contains("path") || !payload["path"].is_string()) {
                throw std::runtime_error("Missing string parameter 'path'");
            }

            std::string path_str = payload["path"];
            std::filesystem::path file_path(path_str);

            if (!std::filesystem::exists(file_path)) {
                return nlohmann::json({
                    {"error", "File does not exist: " + path_str}
                });
            }

            std::ifstream file(file_path);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file");
            }

            std::stringstream buffer;
            buffer << file.rdbuf();

            return nlohmann::json({
                {"content", buffer.str()},
                {"success", true}
            });
        });
    }
}
