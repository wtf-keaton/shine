#include <shine/ipc.hpp>
#include <iostream>

namespace shine::ipc {
    void Router::AddHandler(const std::string &cmd, CommandHandler handler) {
        handlers_[cmd] = std::move(handler);
    }

    void Router::SetAllowedCommands(const std::unordered_set<std::string> &commands) {
        allowedCommands_ = commands;
    }

    std::string Router::Route(const std::string &raw_message) {
        try {
            auto request = nlohmann::json::parse(raw_message);

            if (!request.contains("cmd") || !request["cmd"].is_string()) {
                return "console.error('IPC Error: Message missing string field \"cmd\"');";
            }

            std::string cmd = request["cmd"];

            if (!allowedCommands_.contains(cmd)) {
                std::string err = "Security Block: Command '" + cmd + "' is not listed in capabilities.allowedCommands";
                std::cerr << "[IPC Router] " << err << std::endl;
                return "console.error(\"" + err + "\");";
            }
            nlohmann::json msg_id = request.contains("id") ? request["id"] : nlohmann::json(nullptr);
            nlohmann::json payload = request.contains("payload") ? request["payload"] : nlohmann::json(nullptr);


            auto it = handlers_.find(cmd);
            if (it != handlers_.end()) {
                nlohmann::json response = it->second(payload);

                nlohmann::json full_response = {
                    {"id", msg_id},
                    {"cmd", cmd},
                    {"data", response}
                };

                return "window.__SHINE_IPC_RECEIVE__(" + full_response.dump() + ");";
            } else {
                return "console.warn('IPC Warn: No C++ handler found for cmd: " + cmd + "');";
            }
        } catch (const std::exception &e) {
            std::string err = e.what();

            return "console.error('IPC parse Error: " + err + "'";
        }
    }
}
