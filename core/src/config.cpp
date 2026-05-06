#include <shine/config.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace shine {
    AppConfig AppConfig::Load(const std::string& configStr) {
        AppConfig config;

#ifdef _DEBUG
        std::ifstream ifs(configStr);
        if (!ifs.is_open()) {
            std::cerr << "Could not open file " << configStr << std::endl;
            return config;
        }
#endif

        try {
#ifdef _DEBUG
            auto json_data = nlohmann::json::parse(ifs);
#else
            auto json_data = nlohmann::json::parse(configStr);
#endif
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
                if (w.contains("centered") && w["centered"].is_boolean()) {
                    config.centered = w["centered"];
                }
            }

            if (json_data.contains("capabilities")) {
                config.has_capabilities_policy = true;
                const auto& capabilities = json_data["capabilities"];

                auto read_capability_object = [&config](const nlohmann::json& cap) {
                    if (cap.contains("allowedCommands") && cap["allowedCommands"].is_array()) {
                        for (const auto& cmd : cap["allowedCommands"]) {
                            if (cmd.is_string()) {
                                config.allowed_commands.insert(cmd.get<std::string>());
                            }
                        }
                    }

                    if (cap.contains("permissions") && cap["permissions"].is_array()) {
                        for (const auto& permission : cap["permissions"]) {
                            if (permission.is_string()) {
                                config.allowed_permissions.insert(permission.get<std::string>());
                            }
                        }
                    }
                };

                if (capabilities.is_object()) {
                    read_capability_object(capabilities);
                } else if (capabilities.is_array()) {
                    for (const auto& cap : capabilities) {
                        if (cap.is_object()) {
                            read_capability_object(cap);
                        }
                    }
                }
            }
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "[Shine Error] Failed to parse config JSON: " << e.what() << "\n";
        }

        return config;
    }
}
