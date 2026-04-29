#include <shine/config.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace shine {
    AppConfig AppConfig::Load(const std::string& filepath) {
        AppConfig config;

        std::ifstream ifs(filepath);
        if (!ifs.is_open()) {
            std::cerr << "Could not open file " << filepath << std::endl;
            return config;
        }

        try {
            auto json_data = nlohmann::json::parse(ifs);

            if (json_data.contains("window")) {
                auto& w = json_data["window"];
                if (w.contains("title")) config.title = w["title"];
                if (w.contains("width")) config.width = w["width"];
                if (w.contains("height")) config.height = w["height"];
                if (w.contains("frameless") && w["frameless"].is_boolean()) {
                    config.frameless = w["frameless"];
                }
                if (w.contains("resizable") && w["resizable"].is_boolean()) {
                    config.resizable = w["resizable"];
                }
            }

            if (json_data.contains("capabilities")) {
                auto& cap = json_data["capabilities"];
                if (cap.contains("allowedCommands") && cap["allowedCommands"].is_array()) {
                    for (const auto& cmd : cap["allowedCommands"]) {
                        config.allowed_commands.insert(cmd.get<std::string>());
                    }
                }
            }
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "[Shine Error] Failed to parse config JSON: " << e.what() << "\n";
        }

        return config;
    }
}