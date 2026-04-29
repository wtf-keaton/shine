#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace shine::ipc {
    using CommandHandler = std::function<nlohmann::json(const nlohmann::json& payload)>;

    struct HandlerRegistration {
        const char* name;
        CommandHandler func;
    };

    class Router {
    public:
        void AddHandler(const std::string& cmd, CommandHandler handler);
        void AddHandlers(std::initializer_list<HandlerRegistration> handlers);

        void SetAllowedCommands(const std::unordered_set<std::string>& allowedCommands);

        std::string Route(const std::string& raw_message);

    private:
        std::unordered_map<std::string, CommandHandler> handlers_;
        std::unordered_set<std::string> allowedCommands_;
    };
}