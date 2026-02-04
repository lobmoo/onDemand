#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <mutex>

#include "fastdds_wrapper/fastdds_node.h"
#include "log/logger.h"
#include "fastdds_wrapper_TEST/PerformanceTest.hpp"
#include "fastdds_wrapper_TEST/PerformanceTestPubSubTypes.hpp"

using namespace PerformanceTest;
namespace FastddsWrapper
{

class FastddsNodeTEST : public ::testing::Test
{
protected:
    void SetUp() override { Logger::GetInstance()->Init("", Logger::console, Logger::info, 64, 1); }
    void TearDown() override {}
};

TEST_F(FastddsNodeTEST, sigDefaultNodeSubPubTest)
{
    FastDataNode node(66, "test_reader");
    EXPECT_TRUE(node.isInitialized());

    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "sigDefaultNodeSubPubTest_topic",
        [](const std::string &topic, std::shared_ptr<TestMessage> data) {
            LOG(info) << "Received data on topic: " << topic
                      << " with message: " << data->message_type();
        });
    EXPECT_TRUE(reader != nullptr);

    auto writer =
        node.createDataWriter<TestMessage, TestMessagePubSubType>("sigDefaultNodeSubPubTest_topic");
    EXPECT_TRUE(reader != nullptr);

    TestMessage message;
    message.message_type("Hello, FastDDS!");
    uint32_t write_count = 0;
    while (write_count < 5) {
        if (writer->writeMessage(message)) {
            LOG(info) << "Sent message: " << message.message_type();
            write_count++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// 测试用户显式释放writer后，引用计数是否清0
TEST_F(FastddsNodeTEST, UserReleaseWriterTest)
{
    FastDataNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    // 创建writer
    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer, nullptr);

    // 检查引用计数（应该为1，只有外部持有）
    EXPECT_EQ(writer.use_count(), 1) << "Initial reference count should be 1";

    LOG(info) << "Writer created, use_count = " << writer.use_count();

    // 用户显式释放
    writer.reset();

    LOG(info) << "Writer reset, use_count = " << (writer ? writer.use_count() : 0);

    // 等待一下确保清理
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 现在应该可以重新创建同名topic的writer
    auto writer2 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer2, nullptr) << "Should be able to recreate writer after reset";

    EXPECT_EQ(writer2.use_count(), 1) << "New writer reference count should be 1";

    LOG(info) << "✓ 用户释放后引用计数正确清0，可以重新创建";
}

// 测试用户显式释放reader后，引用计数是否清0
TEST_F(FastddsNodeTEST, UserReleaseReaderTest)
{
    FastDataNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    auto callback = [](const std::string &topic, std::shared_ptr<TestMessage> msg) {
        // 回调函数
    };

    // 创建reader
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>("test_topic", callback);
    ASSERT_NE(reader, nullptr);

    // 检查引用计数
    EXPECT_EQ(reader.use_count(), 1) << "Initial reference count should be 1";

    LOG(info) << "Reader created, use_count = " << reader.use_count();

    // 用户显式释放
    reader.reset();

    LOG(info) << "Reader reset, use_count = " << (reader ? reader.use_count() : 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 应该可以重新创建
    auto reader2 =
        node.createDataReader<TestMessage, TestMessagePubSubType>("test_topic", callback);
    ASSERT_NE(reader2, nullptr) << "Should be able to recreate reader after reset";

    EXPECT_EQ(reader2.use_count(), 1) << "New reader reference count should be 1";

    LOG(info) << "✓ 用户释放reader后引用计数正确清0，可以重新创建";
}

// 测试用户不释放，Node析构时是否正确清理
TEST_F(FastddsNodeTEST, NodeDestructorCleanupTest)
{
    std::weak_ptr<void> writer_weak;
    std::weak_ptr<void> reader_weak;

    {
        // 在作用域内创建node和writer/reader
        FastDataNode temp_node(0, "test_participant");
        ASSERT_TRUE(temp_node.isInitialized());

        auto writer =
            temp_node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic_w");
        ASSERT_NE(writer, nullptr);
        LOG(info) << "Writer created, use_count = " << writer.use_count();

        auto callback = [](const std::string &topic, std::shared_ptr<TestMessage> msg) {};
        auto reader = temp_node.createDataReader<TestMessage, TestMessagePubSubType>("test_topic_r",
                                                                                     callback);
        ASSERT_NE(reader, nullptr);
        LOG(info) << "Reader created, use_count = " << reader.use_count();

        // 保存weak_ptr用于后续检查
        writer_weak = writer;
        reader_weak = reader;

        EXPECT_FALSE(writer_weak.expired()) << "Writer should be alive";
        EXPECT_FALSE(reader_weak.expired()) << "Reader should be alive";

        // 用户持有shared_ptr，不释放
        // temp_node离开作用域时应该析构并清理
        LOG(info) << "Node going out of scope with user still holding writer/reader...";
    }

    // 离开作用域，node已析构
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 检查weak_ptr是否已失效（说明内部已释放）
    EXPECT_TRUE(writer_weak.expired()) << "Writer should be expired after node destruction";
    EXPECT_TRUE(reader_weak.expired()) << "Reader should be expired after node destruction";

    LOG(info) << "✓ Node析构时正确释放了用户未释放的writer/reader";
}

// 测试用户不释放时的引用计数变化
TEST_F(FastddsNodeTEST, ReferenceCountWithoutUserReleaseTest)
{
    std::shared_ptr<FastDDSTopicWriter<TestMessage>> writer;
    std::weak_ptr<void> writer_weak;

    {
        FastDataNode temp_node(0, "test_participant");
        ASSERT_TRUE(temp_node.isInitialized());

        writer = temp_node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
        ASSERT_NE(writer, nullptr);

        // 此时引用计数应该为1（只有外部的writer持有，内部用weak_ptr）
        EXPECT_EQ(writer.use_count(), 1) << "Reference count should be 1 (only external holder)";
        LOG(info) << "Writer use_count (before node destruction) = " << writer.use_count();

        writer_weak = writer;
        EXPECT_FALSE(writer_weak.expired());

        // Node析构
        LOG(info) << "Destroying node...";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Node析构后，外部writer仍然持有对象
    EXPECT_EQ(writer.use_count(), 1) << "Reference count should still be 1 (external holder only)";
    EXPECT_FALSE(writer_weak.expired()) << "Writer should still be alive (held by external)";
    LOG(info) << "Writer use_count (after node destruction) = " << writer.use_count();

    // 用户释放
    writer.reset();

    // 现在应该完全释放
    EXPECT_TRUE(writer_weak.expired()) << "Writer should be expired after user release";
    LOG(info) << "✓ 引用计数变化符合预期：Node使用weak_ptr不影响外部shared_ptr";
}

// 测试重复创建同名writer的保护机制
TEST_F(FastddsNodeTEST, DuplicateWriterProtectionTest)
{
    FastDataNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    // 创建第一个writer
    auto writer1 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer1, nullptr);

    // 尝试创建同名writer（应该失败）
    auto writer2 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    EXPECT_EQ(writer2, nullptr) << "Should not be able to create duplicate writer";

    LOG(info) << "✓ 重复创建同名writer被正确拒绝";

    // 释放第一个writer
    writer1.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 现在应该可以创建
    auto writer3 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer3, nullptr) << "Should be able to create after release";

    LOG(info) << "✓ 释放后可以重新创建同名writer";
}

// 测试多个writer/reader的生命周期管理
TEST_F(FastddsNodeTEST, MultipleWritersReadersTest)
{
    FastDataNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    auto callback = [](const std::string &topic, std::shared_ptr<TestMessage> msg) {};

    // 创建多个writer和reader
    auto writer1 = node.createDataWriter<TestMessage, TestMessagePubSubType>("topic1");
    auto writer2 = node.createDataWriter<TestMessage, TestMessagePubSubType>("topic2");
    auto reader1 = node.createDataReader<TestMessage, TestMessagePubSubType>("topic3", callback);
    auto reader2 = node.createDataReader<TestMessage, TestMessagePubSubType>("topic4", callback);

    ASSERT_NE(writer1, nullptr);
    ASSERT_NE(writer2, nullptr);
    ASSERT_NE(reader1, nullptr);
    ASSERT_NE(reader2, nullptr);

    LOG(info) << "Created 2 writers and 2 readers";
    LOG(info) << "writer1 use_count = " << writer1.use_count();
    LOG(info) << "writer2 use_count = " << writer2.use_count();
    LOG(info) << "reader1 use_count = " << reader1.use_count();
    LOG(info) << "reader2 use_count = " << reader2.use_count();

    // 每个引用计数都应该是1
    EXPECT_EQ(writer1.use_count(), 1);
    EXPECT_EQ(writer2.use_count(), 1);
    EXPECT_EQ(reader1.use_count(), 1);
    EXPECT_EQ(reader2.use_count(), 1);

    // 释放其中一些
    writer1.reset();
    reader1.reset();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 应该可以重新创建
    auto writer1_new = node.createDataWriter<TestMessage, TestMessagePubSubType>("topic1");
    auto reader1_new =
        node.createDataReader<TestMessage, TestMessagePubSubType>("topic3", callback);

    ASSERT_NE(writer1_new, nullptr);
    ASSERT_NE(reader1_new, nullptr);

    LOG(info) << "✓ 多个writer/reader的生命周期管理正确";
}

} // namespace FastddsWrapper
