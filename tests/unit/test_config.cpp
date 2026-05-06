#include <gtest/gtest.h>
#include <shine/config.hpp>
#include <fstream>
#include <filesystem>

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_config_path_ = std::filesystem::temp_directory_path().string() + "/shine_test_config.json";
    }

    void TearDown() override {
        std::filesystem::remove(test_config_path_);
    }

    void WriteTestConfig(const std::string& content) {
        std::ofstream ofs(test_config_path_);
        ofs << content;
    }

    std::string test_config_path_;
};

TEST_F(ConfigTest, DefaultValues) {
    shine::AppConfig config = shine::AppConfig::Load("{}");
    EXPECT_EQ(config.title, "Shine App");
    EXPECT_EQ(config.width, 1024);
    EXPECT_EQ(config.height, 768);
    EXPECT_FALSE(config.frameless);
    EXPECT_TRUE(config.resizable);
    EXPECT_FALSE(config.centered);
    EXPECT_TRUE(config.allowed_commands.empty());
}

TEST_F(ConfigTest, ParseWindowSettings) {
#ifdef _DEBUG
    WriteTestConfig(R"({
        "window": {
            "title": "Test App",
            "width": 1280,
            "height": 800,
            "frameless": true,
            "resizable": false,
            "centered": true
        }
    })");
    shine::AppConfig config = shine::AppConfig::Load(test_config_path_);
#else
    shine::AppConfig config = shine::AppConfig::Load(R"({
        "window": {
            "title": "Test App",
            "width": 1280,
            "height": 800,
            "frameless": true,
            "resizable": false,
            "centered": true
        }
    })");
#endif

    EXPECT_EQ(config.title, "Test App");
    EXPECT_EQ(config.width, 1280);
    EXPECT_EQ(config.height, 800);
    EXPECT_TRUE(config.frameless);
    EXPECT_FALSE(config.resizable);
    EXPECT_TRUE(config.centered);
}

TEST_F(ConfigTest, ParseCapabilities) {
#ifdef _DEBUG
    WriteTestConfig(R"({
        "capabilities": {
            "permissions": ["fs:default", "window:default"],
            "allowedCommands": ["legacy_cmd"]
        }
    })");
    shine::AppConfig config = shine::AppConfig::Load(test_config_path_);
#else
    shine::AppConfig config = shine::AppConfig::Load(R"({
        "capabilities": {
            "permissions": ["fs:default", "window:default"],
            "allowedCommands": ["legacy_cmd"]
        }
    })");
#endif

    EXPECT_TRUE(config.has_capabilities_policy);
    EXPECT_EQ(config.allowed_permissions.size(), 2);
    EXPECT_TRUE(config.allowed_permissions.contains("fs:default"));
    EXPECT_TRUE(config.allowed_permissions.contains("window:default"));
    EXPECT_TRUE(config.allowed_commands.contains("legacy_cmd"));
}

TEST_F(ConfigTest, ParseTauriStyleCapabilityArray) {
#ifdef _DEBUG
    WriteTestConfig(R"({
        "capabilities": [
            {
                "identifier": "main",
                "windows": ["main"],
                "permissions": ["fs:allow-read-text-file", "window:allow-close"]
            }
        ]
    })");
    shine::AppConfig config = shine::AppConfig::Load(test_config_path_);
#else
    shine::AppConfig config = shine::AppConfig::Load(R"({
        "capabilities": [
            {
                "identifier": "main",
                "windows": ["main"],
                "permissions": ["fs:allow-read-text-file", "window:allow-close"]
            }
        ]
    })");
#endif

    EXPECT_TRUE(config.has_capabilities_policy);
    EXPECT_EQ(config.allowed_permissions.size(), 2);
    EXPECT_TRUE(config.allowed_permissions.contains("fs:allow-read-text-file"));
    EXPECT_TRUE(config.allowed_permissions.contains("window:allow-close"));
}

TEST_F(ConfigTest, PartialConfig) {
#ifdef _DEBUG
    WriteTestConfig(R"({"window": {"title": "Partial"}})");
    shine::AppConfig config = shine::AppConfig::Load(test_config_path_);
#else
    shine::AppConfig config = shine::AppConfig::Load(R"({"window": {"title": "Partial"}})");
#endif

    EXPECT_EQ(config.title, "Partial");
    EXPECT_EQ(config.width, 1024);
    EXPECT_EQ(config.height, 768);
}

TEST_F(ConfigTest, InvalidJSON) {
#ifdef _DEBUG
    WriteTestConfig("not json");
    shine::AppConfig config = shine::AppConfig::Load(test_config_path_);
#else
    shine::AppConfig config = shine::AppConfig::Load("not json");
#endif

    EXPECT_EQ(config.title, "Shine App");
}
