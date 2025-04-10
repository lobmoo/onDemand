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

static std::mutex delays_mutex;
class UDPTestFixtureMult : public ::testing::Test
{

private:
    std::vector<std::string> topic_names = {
        "topic1",  "topic2",  "topic3",  "topic4",  "topic5",  "topic6",  "topic7",
        "topic8",  "topic9",  "topic10", "topic11", "topic12", "topic13", "topic14",
        "topic15", "topic16", "topic17", "topic18", "topic19", "topic20",
    };

protected:
    void SetUp() override
    {
        senderNode = std::make_unique<DataNode>(0, "sender_node");
        receiverNode = std::make_unique<DataNode>(0, "receiver_node");
        for (auto topic_name : topic_names) {
            senderNode->registerTopicType<Message_512PubSubType>(topic_name);
            receiverNode->registerTopicType<Message_512PubSubType>(topic_name);

            senderNode->registerTopicType<Message_512PubSubType>(topic_name);
            receiverNode->registerTopicType<Message_512PubSubType>(topic_name);

            senderNode->registerTopicType<Message_2621440PubSubType>(topic_name);
            receiverNode->registerTopicType<Message_2621440PubSubType>(topic_name);

            dataWriter_512[topic_name] = senderNode->createDataWriter<Message_512>(topic_name);
            dataWriter_51200[topic_name] = senderNode->createDataWriter<Message_51200>(topic_name);
            dataWriter_2621440[topic_name] =
                senderNode->createDataWriter<Message_2621440>(topic_name);
        }

        for (auto topic_name : topic_names) {
            auto dataReader_512 = receiverNode->createDataReader<Message_512>(
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
                        delays_512[data->id()].push_back(delay_time);
                    }
                    LOG(debug) << "recv message [" << topic_name << "]: " << data->id()
                               << " recv message delay time: " << delay_time
                               << "data:" << data->data()[0];
                });
            auto dataReader_51200 = receiverNode->createDataReader<Message_51200>(
                "Message_51200",
                [this](const std::string &topic_name, std::shared_ptr<Message_51200> data) {
                    auto now = std::chrono::system_clock::now();
                    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                    auto epoch = value.time_since_epoch();
                    auto timestamp =
                        std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

                    int64_t delay_time = timestamp - data->timestamp();
                    {
                        std::lock_guard<std::mutex> lock(delays_mutex);
                        delays_51200[data->id()].push_back(delay_time);
                    }

                    LOG(debug) << "recv message [" << topic_name << "]: " << data->id()
                               << " recv message delay time: " << delay_time
                               << "data:" << data->data()[0];
                });

            auto dataReader_2621440 = receiverNode->createDataReader<Message_2621440>(
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
                        delays_2621440[data->id()].push_back(delay_time);
                    }

                    LOG(debug) << "recv message [" << topic_name << "]: " << data->id()
                               << " recv message delay time: " << delay_time
                               << "data:" << data->data()[0];
                });
        }
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

    std::unordered_map<std::string, DDSTopicDataWriter<Message_512> *> dataWriter_512;
    std::unordered_map<std::string, DDSTopicDataWriter<Message_51200> *> dataWriter_51200;
    std::unordered_map<std::string, DDSTopicDataWriter<Message_2621440> *> dataWriter_2621440;

    // DDSTopicDataReader<Message_512> *dataReader_512;
    // DDSTopicDataReader<Message_51200> *dataReader_51200;
    // DDSTopicDataReader<Message_2621440> *dataReader_2621440;

    std::unordered_map<int32_t, std::vector<uint64_t>> delays_512;
    std::unordered_map<int32_t, std::vector<uint64_t>> delays_51200;
    std::unordered_map<int32_t, std::vector<uint64_t>> delays_2621440;
};

static void calculateAverageDelay(const std::unordered_map<int32_t, std::vector<uint64_t>> &delays,
                           std::string tag)
{
    std::lock_guard<std::mutex> lock(delays_mutex);
    if (delays.empty()) {
        return;
    }
    for (const auto &[id, delay] : delays) {
        uint64_t sum = 0;
        for (auto delay_time : delay) {
            sum += delay_time;
        }
        if (!delay.empty()) {
            LOG(info) << "Average delay for message [" << tag << "][" << id
                      << "]: " << static_cast<double>(sum) / delay.size();

        } else {
            LOG(warning) << "No messages received for message [" << id << "].";
        }
    }
    return;
}

static int32_t get_hostname()
{
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::hash<std::string> hasher;
        size_t hash_value = hasher(hostname);
        return static_cast<int32_t>(hash_value);
    } else {
        return -1; // 错误处理
    }
}

TEST_F(UDPTestFixtureMult, MultiSenderReceiverTest1k)
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
        message.id(get_hostname());
        std::unique_ptr<std::array<int16_t, 512>> data =
            std::make_unique<std::array<int16_t, 512>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(cnt); // 示例数据
        }
        message.data(*data);

        for(auto &[topic_name, dataWriter] : dataWriter_512) {
            if (dataWriter->writeMessage(message)) {
                LOG(debug) << "send message: " << message.id();
            }
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    calculateAverageDelay(delays_512, "1k");
}

TEST_F(UDPTestFixtureMult, MultiSenderReceiverTest100k)
{
    uint32_t cnt = 0;
    int index = 0;

    while (cnt < 10) {
        Message_51200 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(get_hostname());
        std::unique_ptr<std::array<int16_t, 51200>> data =
            std::make_unique<std::array<int16_t, 51200>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(cnt); // 示例数据
        }
        message.data(*data);
        for(auto &[topic_name, dataWriter] : dataWriter_51200) {
            if (dataWriter->writeMessage(message)) {
                LOG(debug) << "send message: " << message.id();
            }
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    calculateAverageDelay(delays_51200, "100k");
}

TEST_F(UDPTestFixtureMult, MultiSenderReceiverTest5M)
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
        message.id(get_hostname());
        std::unique_ptr<std::array<int16_t, 2621440>> data =
            std::make_unique<std::array<int16_t, 2621440>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(cnt); // 示例数据
        }
        message.data(*data);
        for(auto &[topic_name, dataWriter] : dataWriter_2621440) {
            if (dataWriter->writeMessage(message)) {
                LOG(debug) << "send message: " << message.id();
            }
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    calculateAverageDelay(delays_2621440, "5M");
}
