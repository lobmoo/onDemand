#include "config.h"
#include <gtest/gtest.h>

TEST(ConfigTest, LoadAndSaveConfig) {
    // 创建临时配置文件
    const std::string configContent = R"(
    {
        "name": "John",
        "age": 25,
        "height": 170.5,
        "active": true,
        "user": {
            "name": "Jane",
            "age": 28,
            "height": 165.5
        },
        "ages": [20, 30, 40],
        "names": ["Alice", "Bob"],
        "bools": [true, false]
    }
    )";

    std::string tempConfigFile = "temp_config.json";
    std::ofstream configFile(tempConfigFile);
    configFile << configContent;
    configFile.close();

    Config config(tempConfigFile);

    // 测试加载配置
    config.load();
    EXPECT_TRUE(config.isLoaded());

    // 测试获取基本值
    EXPECT_EQ(config.getInt("age"), 25);
    EXPECT_FLOAT_EQ(config.getFloat("height"), 170.5f);
    EXPECT_EQ(config.getString("name"), "John");
    EXPECT_TRUE(config.getBool("active"));

    // 测试获取嵌套值
    EXPECT_EQ(config.getNestedInt({"user", "age"}), 28);
    EXPECT_EQ(config.getNestedString({"user", "name"}), "Jane");

    // 测试获取数组值
    std::vector<int> ages = config.getIntArray("ages");
    EXPECT_EQ(ages.size(), 3);
    EXPECT_EQ(ages[0], 20);
    EXPECT_EQ(ages[1], 30);
    EXPECT_EQ(ages[2], 40);

    std::vector<std::string> names = config.getStringArray("names");
    EXPECT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "Alice");
    EXPECT_EQ(names[1], "Bob");

    // 测试设置基本值
    config.setInt("age", 30);
    config.setNestedFloat({"user", "height"}, 175.5f);
    config.setIntArray("ages", {25, 35, 45});

    // 测试保存配置
    config.save();

    // 重新加载配置文件，验证保存是否成功
    Config newConfig(tempConfigFile);
    newConfig.load();
    EXPECT_EQ(newConfig.getInt("age"), 30);
    EXPECT_FLOAT_EQ(newConfig.getNestedFloat({"user", "height"}), 175.5f);

    std::vector<int> newAges = newConfig.getIntArray("ages");
    EXPECT_EQ(newAges.size(), 3);
    EXPECT_EQ(newAges[0], 25);
    EXPECT_EQ(newAges[1], 35);
    EXPECT_EQ(newAges[2], 45);

    // 删除临时配置文件
    remove(tempConfigFile.c_str());
}

TEST(ConfigTest, HandleErrors) {
    // 测试加载不存在的配置文件
    Config invalidConfig("invalid_path.json");
    EXPECT_THROW(invalidConfig.load(), std::runtime_error);

    // 测试获取不存在的键
    Config config("valid_config.json");
    EXPECT_THROW(config.load(), std::runtime_error);
    EXPECT_EQ(config.getInt("non_existing_key", 42), 42);
    EXPECT_EQ(config.getString("non_existing_key", "default"), "default");

    // 测试设置嵌套值到不存在的嵌套路径
    config.setNestedInt({"nested", "path", "age"}, 42);
    EXPECT_EQ(config.getNestedInt({"nested", "path", "age"}, 0), 42);

    EXPECT_THROW(config.save("/root/json/"), std::runtime_error);
}

TEST(ConfigTest, NestedArrayTest) {
    // 创建临时配置文件
    const std::string configContent = R"(
    {
        "nested": {
            "array": [1, 2, 3]
        }
    }
    )";

    std::string tempConfigFile = "temp_config.json";
    std::ofstream configFile(tempConfigFile);
    configFile << configContent;
    configFile.close();

    Config config(tempConfigFile);
    config.load();

    // 测试获取嵌套数组
    std::vector<int> array = config.getNestedIntArray({"nested", "array"});
    EXPECT_EQ(array.size(), 3);
    EXPECT_EQ(array[0], 1);
    EXPECT_EQ(array[1], 2);
    EXPECT_EQ(array[2], 3);

    // 删除临时配置文件
    remove(tempConfigFile.c_str());
}

TEST(ConfigTest, BooleanTest) {
    const std::string configContent = R"(
    {
        "isActive": true,
        "isDebug": false
    }
    )";

    std::string tempConfigFile = "temp_config.json";
    std::ofstream configFile(tempConfigFile);
    configFile << configContent;
    configFile.close();

    Config config(tempConfigFile);
    config.load();

    EXPECT_TRUE(config.getBool("isActive"));
    EXPECT_FALSE(config.getBool("isDebug"));

    config.setBool("isActive", false);
    EXPECT_FALSE(config.getBool("isActive", true));

    remove(tempConfigFile.c_str());
}


//todo：嵌套数组后期实现
TEST(ConfigTest, NestedArraysTest) {

    const std::string configContent = R"(
    {
        "users": [
            {
                "name": "Alice",
                "emails": ["alice@example.com", "alice@gmail.com"]
            },
            {
                "name": "Bob",
                "emails": ["bob@example.com", "bob@gmail.com"]
            }
        ]
    }
    )";

    std::string tempConfigFile = "temp_config.json";
    {
        std::ofstream configFile(tempConfigFile);
        configFile << configContent;
    }

    Config config(tempConfigFile);
    config.load();

    std::vector<std::string> emails = config.getNestedStringArray({"users", 0, "emails"});
    EXPECT_EQ(emails.size(), 2);
    EXPECT_EQ(emails[0], "alice@example.com");
    EXPECT_EQ(emails[1], "alice@gmail.com");

}