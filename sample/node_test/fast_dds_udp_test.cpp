#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <gtest/gtest.h>

#include "TestMessage_1k/TestMessage.hpp"
#include "TestMessage_1k/TestMessagePubSubTypes.hpp"

#include "TestMessage_100k/TestMessage100k.hpp"
#include "TestMessage_100k/TestMessage100kPubSubTypes.hpp"

#include "TestMessage_1M/TestMessage1M.hpp"
#include "TestMessage_1M/TestMessage1MPubSubTypes.hpp"

#include "TestMessage_5M/TestMessage5M.hpp"
#include "TestMessage_5M/TestMessage5MPubSubTypes.hpp"

#include "TestMessage_100M/TestMessage100M.hpp"
#include "TestMessage_100M/TestMessage100MPubSubTypes.hpp"

#include "fastdds_wrapper/DataNode.h"
#include "fastdds_wrapper/DDSParticipantListener.h"
#include "log/logger.h"
using namespace std;


std::mutex delays_mutex;
class UDPTestFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        senderNode = std::make_unique<DataNode>(0, "sender_node");
        receiverNode = std::make_unique<DataNode>(0, "receiver_node");

        senderNode->registerTopicType<Message_512PubSubType>("Message_512");
        receiverNode->registerTopicType<Message_512PubSubType>("Message_512");

        senderNode->registerTopicType<Message_2621440PubSubType>("Message_2621440");
        receiverNode->registerTopicType<Message_2621440PubSubType>("Message_2621440");

        // 创建数据写入器和读取器
        dataWriter_512 = senderNode->createDataWriter<Message_512>("Message_512");
        dataReader_512 = receiverNode->createDataReader<Message_512>(
            "Message_512",
            [this](const std::string &topic_name, std::shared_ptr<Message_512> data) {
                auto now = std::chrono::system_clock::now();
                auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                auto epoch = value.time_since_epoch();
                auto timestamp =
                    std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

                int64_t delay_time = timestamp - data->timestamp();
                {
                    std::lock_guard<std::mutex> lock(delays_mutex);
                    delays_512.push_back(delay_time);
                }
                LOG(info) << "recv message [" << topic_name << "]: " << data->id()
                          << " recv message delay time: " << delay_time;
            });

        dataWriter_2621440 = senderNode->createDataWriter<Message_2621440>("Message_2621440");
        dataReader_2621440 = receiverNode->createDataReader<Message_2621440>(
            "Message_2621440",
            [this](const std::string &topic_name, std::shared_ptr<Message_2621440> data) {
                auto now = std::chrono::system_clock::now();
                auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                auto epoch = value.time_since_epoch();
                auto timestamp =
                    std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

                int64_t delay_time = timestamp - data->timestamp();
                {
                    std::lock_guard<std::mutex> lock(delays_mutex);
                    delays_2621440.push_back(delay_time);
                }
                LOG(info) << "recv message [" << topic_name << "]: " << data->id()
                          << " recv message delay time: " << delay_time;
            });
    }

    // 清理函数
    void TearDown() override
    {
        // 销毁节点和其他资源
        senderNode.reset();
        receiverNode.reset();
    }

    // 公共成员变量
    std::unique_ptr<DataNode> senderNode;
    std::unique_ptr<DataNode> receiverNode;
    DDSTopicDataWriter<Message_512> *dataWriter_512;
    DDSTopicDataReader<Message_512> *dataReader_512;
    DDSTopicDataWriter<Message_2621440> *dataWriter_2621440;
    DDSTopicDataReader<Message_2621440> *dataReader_2621440;
    std::vector<uint64_t> delays_512;
    std::vector<uint64_t> delays_2621440;
};

double calculateAverageDelay(const std::vector<uint64_t> &delays)
{
    std::lock_guard<std::mutex> lock(delays_mutex);
    if (delays.empty()) {
        return 0.0;
    }

    uint64_t sum = 0;
    for (auto delay : delays) {
        sum += delay;
    }
    return static_cast<double>(sum) / delays.size();
}

TEST_F(UDPTestFixture, SenderReceiverTest1k)
{
    uint32_t cnt = 0;
    int index = 0;

    while (cnt < 10) {
        Message_512 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(index++);
        std::unique_ptr<std::array<int16_t, 512>> data =
            std::make_unique<std::array<int16_t, 512>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(*data);
        if (dataWriter_512->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    double averageDelay = calculateAverageDelay(delays_512);
    LOG(info) << "Average delay for 1K messages: " << averageDelay;
}

TEST_F(UDPTestFixture, SenderReceiverTest5M)
{
    uint32_t cnt = 0;
    int index = 0;

    while (cnt < 10) {
        Message_2621440 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(index++);
        std::unique_ptr<std::array<int16_t, 2621440>> data =
            std::make_unique<std::array<int16_t, 2621440>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(*data);
        if (dataWriter_2621440->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    double averageDelay = calculateAverageDelay(delays_2621440);
    LOG(info) << "Average delay for 5M messages: " << averageDelay;
}