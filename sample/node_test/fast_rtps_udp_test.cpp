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

TEST(UDPTest, SenderTest1k)
{
    uint32_t cnt = 0;
    int index = 0;

    // 初始化节点
    DataNode node(0, "sender_node");
    node.registerTopicType<Message_512PubSubType>("Message_512");
    // 创建数据写入器
    auto dataWriter = node.createDataWriter<Message_512>("Message_512");
    while (cnt < 10) {
        Message_512 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

        message.timestamp(timestamp);
        message.id(index++);

        // 填充示例数据
        std::array<int16_t, 512> data;
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(data);

        // 发送消息
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }

        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TEST(UDPTest, ReceiverTest1k)
{
    std::vector<uint64_t> delays;

    // 初始化节点
    DataNode node(0, "receiver_node");
    node.registerTopicType<Message_512PubSubType>("Message_512");

    // 创建数据读取器
    auto dataReader = node.createDataReader<Message_512>(
        "Message_512", [&delays](const std::string &topic_name, std::shared_ptr<Message_512> data) {
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

            int64_t delay_time = timestamp - data->timestamp();
            delays.push_back(delay_time);

            LOG(info) << "recv message [" << topic_name << "]: " << data->id()
                      << " recv message delay time: " << delay_time;
        });

    // 等待一段时间以接收所有消息
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 计算平均延迟
    uint64_t sum = 0;
    for (auto delay : delays) {
        sum += delay;
    }

    if (!delays.empty()) {
        LOG(info) << "Average delay: " << sum / delays.size();
    } else {
        LOG(warning) << "No messages received.";
    }
}

TEST(UDPTest, SenderTest5M)
{
    sleep(3);
    uint32_t cnt = 0;
    int index = 0;

    // 初始化节点
    DataNode node(0, "sender_node");
    node.registerTopicType<Message_2621440PubSubType>("Message_2621440");
    // 创建数据写入器
    auto dataWriter = node.createDataWriter<Message_2621440>("Message_2621440");
    while (cnt < 10) {
        Message_2621440 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

        message.timestamp(timestamp);
        message.id(index++);

        // 填充示例数据

        std::unique_ptr<std::array<int16_t, 2621440>> data =
            std::make_unique<std::array<int16_t, 2621440>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(*data);

        // 发送消息
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }

        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TEST(UDPTest, ReceiverTest5M)
{
    std::vector<uint64_t> delays;

    // 初始化节点
    DataNode node(0, "receiver_node");
    node.registerTopicType<Message_2621440PubSubType>("Message_2621440");

    // 创建数据读取器
    auto dataReader = node.createDataReader<Message_2621440>(
        "Message_2621440",
        [&delays](const std::string &topic_name, std::shared_ptr<Message_2621440> data) {
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

            int64_t delay_time = timestamp - data->timestamp();
            delays.push_back(delay_time);

            LOG(info) << "recv message [" << topic_name << "]: " << data->id()
                      << " recv message delay time: " << delay_time;
        });

    // 等待一段时间以接收所有消息
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 计算平均延迟
    uint64_t sum = 0;
    for (auto delay : delays) {
        sum += delay;
    }

    if (!delays.empty()) {
        LOG(info) << "Average delay: " << sum / delays.size();
    } else {
        LOG(warning) << "No messages received.";
    }
}

TEST(UDPTest, MultiTopicSenderTest1k)
{
    sleep(3);
    uint32_t cnt = 0;
    int index = 0;

    // 初始化节点
    DataNode node(0, "sender_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    for (const auto &topic : topics) {
        node.registerTopicType<Message_512PubSubType>(topic);
    }

    // 创建多个数据写入器
    std::unordered_map<std::string, DDSTopicDataWriter<Message_512> *> dataWriters;

    for (const auto &topic : topics) {
        dataWriters[topic] = node.createDataWriter<Message_512>(topic);
    }

    while (cnt < 10) {
        for (const auto &topic : topics) {
            Message_512 message;
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

            message.timestamp(timestamp);
            message.id(index++);

            // 填充示例数据
            std::array<int16_t, 512> data;
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] = static_cast<int16_t>(i); // 示例数据
            }
            message.data(data);

            // 发送消息
            if (dataWriters[topic]->writeMessage(message)) {
                LOG(info) << "send message to [" << topic << "]: " << message.id();
            }
        }

        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TEST(UDPTest, MultiTopicReceiverTest1k)
{
    // 存储每个主题的延迟
    std::unordered_map<std::string, std::vector<uint64_t>> topicDelays;

    // 初始化节点
    DataNode node(0, "receiver_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    node.registerTopicType<Message_512PubSubType>(topics[0]);
    node.registerTopicType<Message_512PubSubType>(topics[1]);
    node.registerTopicType<Message_512PubSubType>(topics[2]);
    for (const auto &topic : topics) {
        // 创建数据读取器
        node.createDataReader<Message_512>(topic, [&topicDelays,
                                                   topic](const std::string &topic_name,
                                                          std::shared_ptr<Message_512> data) {
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

            int64_t delay_time = timestamp - data->timestamp();
            topicDelays[topic].push_back(delay_time);

            LOG(info) << "recv message from [" << topic_name << "]: " << data->id()
                      << " recv message delay time: " << delay_time;
        });
    }

    // 等待一段时间以接收所有消息
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 计算每个主题的平均延迟
    for (const auto &[topic, delays] : topicDelays) {
        uint64_t sum = 0;
        for (auto delay : delays) {
            sum += delay;
        }

        if (!delays.empty()) {
            LOG(info) << "Average delay for topic [" << topic << "]: " << sum / delays.size();
        } else {
            LOG(warning) << "No messages received for topic [" << topic << "].";
        }
    }
}

TEST(UDPTest, MultiTopicSenderTest5M)
{
    sleep(3);
    uint32_t cnt = 0;
    int index = 0;

    // 初始化节点
    DataNode node(0, "sender_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    for (const auto &topic : topics) {
        node.registerTopicType<Message_2621440PubSubType>(topic);
    }

    // 创建多个数据写入器
    std::unordered_map<std::string, DDSTopicDataWriter<Message_2621440> *> dataWriters;

    for (const auto &topic : topics) {
        dataWriters[topic] = node.createDataWriter<Message_2621440>(topic);
    }

    while (cnt < 10) {
        for (const auto &topic : topics) {
            Message_2621440 message;
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

            message.timestamp(timestamp);
            message.id(index++);

            // 填充示例数据
            std::unique_ptr<std::array<int16_t, 2621440>> data =
                std::make_unique<std::array<int16_t, 2621440>>();
            for (size_t i = 0; i < data->size(); ++i) {
                (*data)[i] = static_cast<int16_t>(i); // 示例数据
            }
            message.data(*data);

            // 发送消息
            if (dataWriters[topic]->writeMessage(message)) {
                LOG(info) << "send message to [" << topic << "]: " << message.id();
            }
        }

        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TEST(UDPTest, MultiTopicReceiverTest5M)
{
    // 存储每个主题的延迟
    std::unordered_map<std::string, std::vector<uint64_t>> topicDelays;

    // 初始化节点
    DataNode node(0, "receiver_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    node.registerTopicType<Message_2621440PubSubType>(topics[0]);
    node.registerTopicType<Message_2621440PubSubType>(topics[1]);
    node.registerTopicType<Message_2621440PubSubType>(topics[2]);
    for (const auto &topic : topics) {
        // 创建数据读取器
        node.createDataReader<Message_2621440>(topic, [&topicDelays, topic](
                                                          const std::string &topic_name,
                                                          std::shared_ptr<Message_2621440> data) {
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

            int64_t delay_time = timestamp - data->timestamp();
            topicDelays[topic].push_back(delay_time);

            LOG(info) << "recv message from [" << topic_name << "]: " << data->id()
                      << " recv message delay time: " << delay_time;
        });
    }

    // 等待一段时间以接收所有消息
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 计算每个主题的平均延迟
    for (const auto &[topic, delays] : topicDelays) {
        uint64_t sum = 0;
        for (auto delay : delays) {
            sum += delay;
        }

        if (!delays.empty()) {
            LOG(info) << "Average delay for topic [" << topic << "]: " << sum / delays.size();
        } else {
            LOG(warning) << "No messages received for topic [" << topic << "].";
        }
    }
}