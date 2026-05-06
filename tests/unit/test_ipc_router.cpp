#include <gtest/gtest.h>
#include <shine/ipc.hpp>

using shine::ipc::Router;
using shine::ipc::CommandHandler;

static std::string EchoHandler(const nlohmann::json& payload) {
    return payload.dump();
}

TEST(IPCRouter, RouteMessageWithHandler) {
    Router router;
    router.AddHandler("ping", EchoHandler);

    nlohmann::json response = router.Route(R"({"id":1,"cmd":"ping","payload":"hello"})");

    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["cmd"], "ping");
    EXPECT_EQ(response["data"], R"("hello")");
}

TEST(IPCRouter, RejectUnknownCommand) {
    Router router;
    router.AddProtectedHandler("known", EchoHandler);
    router.SetAllowedCommands({"known"});

    nlohmann::json result = router.Route(R"({"id":2,"cmd":"unknown","payload":null})");

    EXPECT_TRUE(result["data"]["error"].get<std::string>().find("Security Block") != std::string::npos);
    EXPECT_TRUE(result["data"]["error"].get<std::string>().find("unknown") != std::string::npos);
}

TEST(IPCRouter, RejectMalformedJSON) {
    Router router;

    nlohmann::json result = router.Route("not json");

    EXPECT_TRUE(result["data"]["error"].get<std::string>().find("parse Error") != std::string::npos);
}

TEST(IPCRouter, RejectMissingCmdField) {
    Router router;
    router.AddHandler("test", EchoHandler);

    nlohmann::json result = router.Route(R"({"id":3,"payload":null})");

    EXPECT_TRUE(result["data"]["error"].get<std::string>().find("IPC Error") != std::string::npos);
}

TEST(IPCRouter, SecurityBlock) {
    Router router;
    router.AddProtectedHandler("allowed_cmd", EchoHandler);
    router.AddProtectedHandler("blocked_cmd", EchoHandler);

    std::unordered_set<std::string> allowed = {"allowed_cmd"};
    router.SetAllowedCommands(allowed);

    nlohmann::json result = router.Route(R"({"id":4,"cmd":"blocked_cmd","payload":null})");

    EXPECT_TRUE(result["data"]["error"].get<std::string>().find("Security Block") != std::string::npos);
}

TEST(IPCRouter, PermissionAllowsRegisteredCommands) {
    Router router;
    router.SetAllowedPermissions({"fs:default"});
    router.AddPermission("fs:default", {"fs_read_text_file"});
    router.AddProtectedHandler("fs_read_text_file", EchoHandler);

    nlohmann::json response = router.Route(R"({"id":9,"cmd":"fs_read_text_file","payload":"ok"})");

    EXPECT_EQ(response["data"], R"("ok")");
}

TEST(IPCRouter, PermissionRegisteredBeforePolicy) {
    Router router;
    router.AddPermission("window:default", {"window_drag", "close"});
    router.SetAllowedPermissions({"window:default"});
    router.AddProtectedHandler("close", EchoHandler);

    nlohmann::json response = router.Route(R"({"id":10,"cmd":"close","payload":null})");

    EXPECT_EQ(response["cmd"], "close");
    EXPECT_EQ(response["data"], "null");
}

TEST(IPCRouter, UnknownPermissionDoesNotAllowCommand) {
    Router router;
    router.SetAllowedPermissions({"fs:default"});
    router.AddPermission("window:default", {"close"});
    router.AddProtectedHandler("close", EchoHandler);

    nlohmann::json response = router.Route(R"({"id":11,"cmd":"close","payload":null})");

    EXPECT_TRUE(response["data"]["error"].get<std::string>().find("Security Block") != std::string::npos);
}

TEST(IPCRouter, AllowedCommandPasses) {
    Router router;
    router.AddProtectedHandler("allowed_cmd", EchoHandler);

    std::unordered_set<std::string> allowed = {"allowed_cmd"};
    router.SetAllowedCommands(allowed);

    nlohmann::json response = router.Route(R"({"id":5,"cmd":"allowed_cmd","payload":{"key":"value"}})");

    EXPECT_EQ(response["cmd"], "allowed_cmd");
    EXPECT_EQ(response["data"], R"({"key":"value"})");
}

TEST(IPCRouter, UserCommandPassesWithoutExplicitCapability) {
    Router router;
    router.SetAllowedPermissions({"fs:default"});
    router.AddHandler("greet", EchoHandler);

    nlohmann::json response = router.Route(R"({"id":12,"cmd":"greet","payload":{"name":"Ada"}})");

    EXPECT_EQ(response["cmd"], "greet");
    EXPECT_EQ(response["data"], R"({"name":"Ada"})");
}

TEST(IPCRouter, AddHandlersInitializerList) {
    Router router;
    router.AddHandlers({
        {"cmd1", EchoHandler},
        {"cmd2", EchoHandler}
    });

    nlohmann::json result1 = router.Route(R"({"id":6,"cmd":"cmd1","payload":"a"})");
    nlohmann::json result2 = router.Route(R"({"id":7,"cmd":"cmd2","payload":"b"})");

    EXPECT_EQ(result1["data"], R"("a")");
    EXPECT_EQ(result2["data"], R"("b")");
}

TEST(IPCRouter, NullPayload) {
    Router router;
    router.AddHandler("test", EchoHandler);

    nlohmann::json response = router.Route(R"({"id":8,"cmd":"test","payload":null})");

    EXPECT_EQ(response["data"], "null");
}
