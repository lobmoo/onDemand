#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "HelloWorldOne.hpp"
#include "HelloWorldOnePubSubTypes.hpp"
#include "fastdds_wrapper/DataNode.h"
#include "fastdds_wrapper/DDSParticipantListener.h"
#include "log/logger.h"
using namespace std;

void run_dds_data_writer();
void run_dds_data_reader();
void run_dds_data_Multiwriter();
void run_dds_data_Multireader();


ParticipantQosHandler qos_configurator()
{
    ParticipantQosHandler handler("test");
    handler.add_statistics_and_monitor();
    return handler;
}

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
    }
    else if (strcmp(argv[1], "mpub") == 0) {
        run_dds_data_Multiwriter();
    } else if (strcmp(argv[1], "msub") == 0) {
        run_dds_data_Multireader();
    }
    else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }
    while (std::cin.get() != '\n') {
    }
    return;
}


int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    test_multi_sub_pub(argc, argv);
}

void run_dds_data_writer()
{
    DDSParticipantListener *listener = new DDSParticipantListener();
    //DataNode node("/home/wwk/workspaces/test_demo/sample/node_example/qosConfig.xml", listener);
    DataNode node(0, "test_writer", qos_configurator);
    //DataNode node(100, "test_writer");
    node.registerTopicType<HelloWorldOnePubSubType>("wwk");

    // eprosima::fastdds::dds::DataWriterQos dataWriterQos;
    // dataWriterQos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
    auto dataWriter = node.createDataWriter<HelloWorldOne>("wwk");
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
    DDSParticipantListener *listener = new DDSParticipantListener();
    //DataNode node("/home/wwk/workspaces/test_demo/sample/node_example/qosConfig.xml", listener);

    DataNode node(0, "test_reader", qos_configurator);
    //DataNode node(100, "test_reader");
    node.registerTopicType<HelloWorldOnePubSubType>("wwk");
    auto dataReader = node.createDataReader<HelloWorldOne>("wwk", processHelloWorldOne);
    while (std::cin.get() != '\n') {
    }
}

void run_dds_data_Multiwriter()
{
    uint32_t cnt = 0;
    int index = 0;
    bool runFlag = true;

    // 初始化节点
    DataNode node(100, "sender_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    for (const auto &topic : topics) {
        node.registerTopicType<HelloWorldOnePubSubType>(topic);
    }

    // 创建多个数据写入器
    std::unordered_map<std::string,  std::shared_ptr<DDSTopicDataWriter<HelloWorldOne>>> dataWriters;

    for (const auto &topic : topics) {
        dataWriters[topic] = node.createDataWriter<HelloWorldOne>(topic);
    }

    std::thread([&]() {
        while (std::cin.get() != '\n') {
        }
        runFlag = false;
        
    }).detach();

    while (cnt < 100000 && runFlag) {
        for (const auto &topic : topics) {
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
    // 存储每个主题的延迟
    std::unordered_map<std::string, std::vector<uint64_t>> topicDelays;

    // 初始化节点
    DataNode node(100, "receiver_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    for (const auto &topic : topics) {
        node.registerTopicType<HelloWorldOnePubSubType>(topic);
        // 创建数据读取器
        node.createDataReader<HelloWorldOne>(
            topic, [](const std::string &topic_name, std::shared_ptr<HelloWorldOne> data) {
                LOG(info) << "recv message from [" << topic_name << "]: " << data->index();
            });
    }

    while (std::cin.get() != '\n') {
    }
}