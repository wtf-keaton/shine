#pragma once

#include <nlohmann/json.hpp>
#include <shine/ipc.hpp>

#define SHINE_COMMAND(name) \
        nlohmann::json name(const nlohmann::json& args)

#define SHINE_HANDLER(func) \
        shine::ipc::HandlerRegistration{#func, func}

#define SHINE_PERMISSION(name, ...) \
        shine::ipc::PermissionRegistration{name, {__VA_ARGS__}}
