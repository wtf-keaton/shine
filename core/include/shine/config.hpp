#pragma once

#include <string>
#include <unordered_set>
#include <cstdint>

namespace shine {
    struct AppConfig {
        std::string title = "Shine App";
        uint32_t width = 1024;
        uint32_t height = 768;
        bool frameless = false;
        bool resizable = true;
        bool centered = false;

        std::unordered_set<std::string> allowed_commands;
        std::unordered_set<std::string> allowed_permissions;
        bool has_capabilities_policy = false;

        static AppConfig Load(const std::string& filepath);
    };
}
