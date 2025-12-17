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

} // namespace FastddsWrapper