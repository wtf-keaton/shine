#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace shine::ipc {
    using CommandHandler = std::function<nlohmann::json(const nlohmann::json& payload)>;

    class Router {
    public:
        void AddHandler(const std::string& cmd, CommandHandler handler);

        void SetAllowedCommands(const std::unordered_set<std::string>& allowedCommands);

        std::string Route(const std::string& raw_message);

    private:
        std::unordered_map<std::string, CommandHandler> handlers_;
        std::unordered_set<std::string> allowedCommands_;
    };
}