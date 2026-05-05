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

    std::string result = router.Route(R"({"id":1,"cmd":"ping","payload":"hello"})");

    nlohmann::json response = nlohmann::json::parse(result.substr(result.find('(') + 1, result.rfind(')') - result.find('(') - 1));
    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["cmd"], "ping");
    EXPECT_EQ(response["data"], R"("hello")");
}

TEST(IPCRouter, RejectUnknownCommand) {
    Router router;
    router.AddHandler("known", EchoHandler);

    std::string result = router.Route(R"({"id":2,"cmd":"unknown","payload":null})");

    EXPECT_TRUE(result.find("Security Block") != std::string::npos);
    EXPECT_TRUE(result.find("unknown") != std::string::npos);
}

TEST(IPCRouter, RejectMalformedJSON) {
    Router router;

    std::string result = router.Route("not json");

    EXPECT_TRUE(result.find("console.error") != std::string::npos);
    EXPECT_TRUE(result.find("parse Error") != std::string::npos);
}

TEST(IPCRouter, RejectMissingCmdField) {
    Router router;
    router.AddHandler("test", EchoHandler);

    std::string result = router.Route(R"({"id":3,"payload":null})");

    EXPECT_TRUE(result.find("console.error") != std::string::npos);
    EXPECT_TRUE(result.find("IPC Error") != std::string::npos);
}

TEST(IPCRouter, SecurityBlock) {
    Router router;
    router.AddHandler("allowed_cmd", EchoHandler);
    router.AddHandler("blocked_cmd", EchoHandler);

    std::unordered_set<std::string> allowed = {"allowed_cmd"};
    router.SetAllowedCommands(allowed);

    std::string result = router.Route(R"({"id":4,"cmd":"blocked_cmd","payload":null})");

    EXPECT_TRUE(result.find("Security Block") != std::string::npos);
}

TEST(IPCRouter, AllowedCommandPasses) {
    Router router;
    router.AddHandler("allowed_cmd", EchoHandler);

    std::unordered_set<std::string> allowed = {"allowed_cmd"};
    router.SetAllowedCommands(allowed);

    std::string result = router.Route(R"({"id":5,"cmd":"allowed_cmd","payload":{"key":"value"}})");

    nlohmann::json response = nlohmann::json::parse(result.substr(result.find('(') + 1, result.rfind(')') - result.find('(') - 1));
    EXPECT_EQ(response["cmd"], "allowed_cmd");
    EXPECT_EQ(response["data"], R"({"key":"value"})");
}

TEST(IPCRouter, AddHandlersInitializerList) {
    Router router;
    router.AddHandlers({
        {"cmd1", EchoHandler},
        {"cmd2", EchoHandler}
    });

    std::string result1 = router.Route(R"({"id":6,"cmd":"cmd1","payload":"a"})");
    std::string result2 = router.Route(R"({"id":7,"cmd":"cmd2","payload":"b"})");

    EXPECT_TRUE(result1.find("a") != std::string::npos);
    EXPECT_TRUE(result2.find("b") != std::string::npos);
}

TEST(IPCRouter, NullPayload) {
    Router router;
    router.AddHandler("test", EchoHandler);

    std::string result = router.Route(R"({"id":8,"cmd":"test","payload":null})");

    nlohmann::json response = nlohmann::json::parse(result.substr(result.find('(') + 1, result.rfind(')') - result.find('(') - 1));
    EXPECT_EQ(response["data"], "null");
}
