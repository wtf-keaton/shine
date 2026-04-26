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

        std::unordered_set<std::string> allowed_commands;

        static AppConfig Load(const std::string& filepath);
    };
}