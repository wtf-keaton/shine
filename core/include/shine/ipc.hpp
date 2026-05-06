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

    struct PermissionRegistration {
        const char* name;
        std::initializer_list<const char*> commands;
    };

    class Router {
    public:
        void AddHandler(const std::string& cmd, CommandHandler handler);
        void AddProtectedHandler(const std::string& cmd, CommandHandler handler);
        void AddHandlers(std::initializer_list<HandlerRegistration> handlers);
        void AddProtectedHandlers(std::initializer_list<HandlerRegistration> handlers);
        void AddPermission(const std::string& permission, std::initializer_list<const char*> commands);
        void AddPermissions(std::initializer_list<PermissionRegistration> permissions);

        void SetAllowedCommands(const std::unordered_set<std::string>& allowedCommands);
        void SetAllowedPermissions(const std::unordered_set<std::string>& allowedPermissions);

        nlohmann::json Route(const std::string& raw_message);

    private:
        bool IsCommandAllowed(const std::string& cmd) const;
        void ResolveAllowedPermissions();

        std::unordered_map<std::string, CommandHandler> handlers_;
        std::unordered_map<std::string, std::unordered_set<std::string>> permissions_;
        std::unordered_set<std::string> userCommands_;
        std::unordered_set<std::string> allowedCommands_;
        std::unordered_set<std::string> allowedPermissions_;
        bool policyConfigured_ = false;
    };
}
