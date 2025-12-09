#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>

#include "HelloWorldOne.hpp"
#include "HelloWorldOnePubSubTypes.hpp"
#include "fastdds_wrapper/fastdds_node.h"
#include "fastdds_wrapper/fastdds_qos_config.h"
#include "log/logger.h"

using namespace std;
using namespace FastddsWrapper;

void run_dds_data_writer();
void run_dds_data_reader();
void run_dds_data_Multiwriter();
void run_dds_data_Multireader();

void processHelloWorldOne(const std::string &topic_name, std::shared_ptr<HelloWorldOne> data)
{
    LOG(info) << "recv message [" << topic_name << "]: " << data->index();
}

void test_multi_sub_pub(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " sub/pub  or msub/mpub" << std::endl;
        return;
    }
    if (strcmp(argv[1], "sub") == 0) {
        run_dds_data_reader();
    } else if (strcmp(argv[1], "pub") == 0) {
        run_dds_data_writer();
    } else if (strcmp(argv[1], "mpub") == 0) {
        run_dds_data_Multiwriter();
    } else if (strcmp(argv[1], "msub") == 0) {
        run_dds_data_Multireader();
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }
    while (std::cin.get() != '\n') {
    }
    return;
}

int main(int argc, char *argv[])
{
    Logger::GetInstance()->Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    test_multi_sub_pub(argc, argv);
}

void run_dds_data_writer()
{
    ParticipantListener *listener = new ParticipantListener();

    ParticipantQoSBuilder qos_configurator;
    qos_configurator.setDiscoveryMulticastLocator("239.255.0.1", 7400)
        .setUserMulticastLocator("239.255.0.1", 7401);
    
    FastDataNode node(10, "test_writer", qos_configurator, listener);

    // 配置 DataWriter QoS
    DataWriterQoSBuilder writer_qos;
    writer_qos.setDurabilityKind(DurabilityKind::TRANSIENT_LOCAL)
        .setReliabilityKind(ReliabilityKind::RELIABLE)
        .setHistoryKind(HistoryKind::KEEP_ALL);

    auto dataWriter = node.createDataWriter<HelloWorldOne, HelloWorldOnePubSubType>("wwk", writer_qos);
    if (!dataWriter) {
        LOG(error) << "Failed to create DataWriter";
        return;
    }

    bool runFlag = true;
    int index = 0;
    std::thread([&]() {
        while (std::cin.get() != '\n') {
        }
        runFlag = false;
    }).detach();

    while (runFlag) {
        HelloWorldOne message;
        message.index(++index);
        message.points(std::vector<uint8_t>(100));
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.index();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void run_dds_data_reader()
{
    ParticipantListener *listener = new ParticipantListener();

    ParticipantQoSBuilder qos_configurator;
    qos_configurator.setDiscoveryMulticastLocator("239.255.0.1", 7400)
        .setUserMulticastLocator("239.255.0.1", 7401);
    
    FastDataNode node(10, "test_reader", qos_configurator, listener);

    // 配置 DataReader QoS
    DataReaderQoSBuilder reader_qos;
    reader_qos.setDurabilityKind(DurabilityKind::TRANSIENT_LOCAL)
        .setReliabilityKind(ReliabilityKind::RELIABLE)
        .setHistoryKind(HistoryKind::KEEP_ALL);

    auto dataReader = node.createDataReader<HelloWorldOne, HelloWorldOnePubSubType>(
        "wwk", processHelloWorldOne, reader_qos);
    if (!dataReader) {
        LOG(error) << "Failed to create DataReader";
        return;
    }

    while (std::cin.get() != '\n') {
    }
}

void run_dds_data_Multiwriter()
{
    uint32_t cnt = 0;
    int index = 0;
    bool runFlag = true;

    // 初始化节点
    FastDataNode node(100, "sender_node");

    // 创建多个数据写入器
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    std::unordered_map<std::string, std::shared_ptr<FastDDSTopicWriter<HelloWorldOne>>> dataWriters;

    for (const auto &topic : topics) {
        // 创建 DataWriter (内部会自动注册类型和创建主题)
        auto writer = node.createDataWriter<HelloWorldOne, HelloWorldOnePubSubType>(topic);
        if (writer) {
            dataWriters[topic] = writer;
        } else {
            LOG(error) << "Failed to create DataWriter for topic: " << topic;
        }
    }

    std::thread([&]() {
        while (std::cin.get() != '\n') {
        }
        runFlag = false;
    }).detach();

    while (cnt < 100000 && runFlag) {
        for (const auto &topic : topics) {
            if (dataWriters.find(topic) == dataWriters.end()) {
                continue;
            }

            HelloWorldOne message;
            message.index(++index);
            message.points(std::vector<uint8_t>(100));

            // 发送消息
            if (dataWriters[topic]->writeMessage(message)) {
                LOG(info) << "send message to [" << topic << "]: " << message.index();
            }
        }

        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void run_dds_data_Multireader()
{
    // 初始化节点
    FastDataNode node(100, "receiver_node");
    std::vector<std::shared_ptr<FastDDSTopicReader<HelloWorldOne>>> readers;

    // 创建多个主题的读取器
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    for (const auto &topic : topics) {
        // 创建 DataReader (内部会自动注册类型和创建主题)
        auto reader = node.createDataReader<HelloWorldOne, HelloWorldOnePubSubType>(
            topic, [](const std::string &topic_name, std::shared_ptr<HelloWorldOne> data) {
                LOG(info) << "recv message from [" << topic_name << "]: " << data->index();
            });

        if (reader) {
            readers.push_back(reader);
        } else {
            LOG(error) << "Failed to create DataReader for topic: " << topic;
        }
    }

    while (std::cin.get() != '\n') {
    }
}
