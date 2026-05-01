#include <shine/ipc.hpp>
#include <iostream>

namespace shine::ipc {
    void Router::AddHandler(const std::string &cmd, CommandHandler handler) {
        handlers_[cmd] = std::move(handler);
        allowedCommands_.insert(cmd);
    }

    void Router::AddHandlers(const std::initializer_list<HandlerRegistration> handlers) {
        for (const auto&[name, func] : handlers) {
            AddHandler(name, func);
        }
    }

    void Router::SetAllowedCommands(const std::unordered_set<std::string> &allowedCommands) {
        allowedCommands_ = allowedCommands;
    }

    std::string Router::Route(const std::string &raw_message) {
        try {
            auto request = nlohmann::json::parse(raw_message);

            if (!request.contains("cmd") || !request["cmd"].is_string()) {
                return "console.error('IPC Error: Message missing string field \"cmd\"');";
            }

            std::string cmd = request["cmd"];

            if (!allowedCommands_.contains(cmd)) {
                const std::string err = "Security Block: Command '" + cmd + "' is not listed in capabilities.allowedCommands";
                std::cerr << "[IPC Router] " << err << std::endl;
                return "console.error(\"" + err + "\");";
            }
            nlohmann::json msg_id = request.contains("id") ? request["id"] : nlohmann::json(nullptr);
            const nlohmann::json payload = request.contains("payload") ? request["payload"] : nlohmann::json(nullptr);


            const auto it = handlers_.find(cmd);
            if (it != handlers_.end()) {
                nlohmann::json response = it->second(payload);

                const nlohmann::json full_response = {
                    {"id", msg_id},
                    {"cmd", cmd},
                    {"data", response}
                };

                return "window.__SHINE_IPC_RECEIVE__(" + full_response.dump() + ");";
            }

            return "console.warn('IPC Warn: No C++ handler found for cmd: " + cmd + "');";
        } catch (const std::exception &e) {
            const std::string err = e.what();

            return "console.error('IPC parse Error: " + err + "'";
        }
    }
}
