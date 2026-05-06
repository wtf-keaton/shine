#include <shine/ipc.hpp>

#include <iostream>

namespace shine::ipc {
    namespace {
        nlohmann::json MakeErrorResponse(nlohmann::json id, nlohmann::json cmd, const std::string& message) {
            return {
                {"id", std::move(id)},
                {"cmd", std::move(cmd)},
                {"data", {
                    {"error", message}
                }}
            };
        }
    }

    void Router::AddHandler(const std::string &cmd, CommandHandler handler) {
        userCommands_.insert(cmd);
        AddProtectedHandler(cmd, std::move(handler));
    }

    void Router::AddProtectedHandler(const std::string& cmd, CommandHandler handler) {
        handlers_[cmd] = std::move(handler);
    }

    void Router::AddHandlers(const std::initializer_list<HandlerRegistration> handlers) {
        for (const auto&[name, func] : handlers) {
            AddHandler(name, func);
        }
    }

    void Router::AddProtectedHandlers(const std::initializer_list<HandlerRegistration> handlers) {
        for (const auto&[name, func] : handlers) {
            AddProtectedHandler(name, func);
        }
    }

    void Router::AddPermission(const std::string& permission, std::initializer_list<const char*> commands) {
        auto& registeredCommands = permissions_[permission];
        for (const char* command : commands) {
            if (command) {
                registeredCommands.insert(command);
            }
        }

        if (allowedPermissions_.contains(permission)) {
            allowedCommands_.insert(registeredCommands.begin(), registeredCommands.end());
        }
    }

    void Router::AddPermissions(const std::initializer_list<PermissionRegistration> permissions) {
        for (const auto& permission : permissions) {
            AddPermission(permission.name, permission.commands);
        }
    }

    void Router::SetAllowedCommands(const std::unordered_set<std::string> &allowedCommands) {
        policyConfigured_ = true;
        allowedCommands_ = allowedCommands;
        ResolveAllowedPermissions();
    }

    void Router::SetAllowedPermissions(const std::unordered_set<std::string>& allowedPermissions) {
        policyConfigured_ = true;
        allowedPermissions_ = allowedPermissions;
        ResolveAllowedPermissions();
    }

    bool Router::IsCommandAllowed(const std::string& cmd) const {
        return userCommands_.contains(cmd) || !policyConfigured_ || allowedCommands_.contains(cmd);
    }

    void Router::ResolveAllowedPermissions() {
        for (const auto& permission : allowedPermissions_) {
            const auto it = permissions_.find(permission);
            if (it != permissions_.end()) {
                allowedCommands_.insert(it->second.begin(), it->second.end());
            }
        }
    }

    nlohmann::json Router::Route(const std::string &raw_message) {
        try {
            auto request = nlohmann::json::parse(raw_message);
            nlohmann::json msg_id = request.value("id", nlohmann::json(nullptr));

            if (!request.contains("cmd") || !request["cmd"].is_string()) {
                return MakeErrorResponse(
                    std::move(msg_id),
                    nullptr,
                    "IPC Error: Message missing string field \"cmd\"");
            }

            std::string cmd = request["cmd"];

            if (!IsCommandAllowed(cmd)) {
                const std::string err =
                    "Security Block: Command '" + cmd + "' is not allowed by configured capabilities";
                std::cerr << "[IPC Router] " << err << std::endl;
                return MakeErrorResponse(std::move(msg_id), cmd, err);
            }

            const nlohmann::json payload = request.value("payload", nlohmann::json(nullptr));
            const auto it = handlers_.find(cmd);

            if (it == handlers_.end()) {
                return MakeErrorResponse(
                    std::move(msg_id),
                    cmd,
                    "IPC Warn: No C++ handler found for cmd: " + cmd);
            }

            return {
                {"id", std::move(msg_id)},
                {"cmd", cmd},
                {"data", it->second(payload)}
            };
        } catch (const std::exception &e) {
            return MakeErrorResponse(
                nullptr,
                nullptr,
                std::string("IPC parse Error: ") + e.what());
        }
    }
}
