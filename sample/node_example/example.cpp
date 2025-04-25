#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

#include "HelloWorldOne.hpp"
#include "HelloWorldOnePubSubTypes.hpp"
#include "fastdds_wrapper/DataNode.h"
#include "fastdds_wrapper/DDSParticipantListener.h"
#include "log/logger.h"
using namespace std;

void run_dds_data_writer();
void run_dds_data_reader();
void run_dds_data_Multireader();
void run_dds_data_Multiwriter();

ParticipantQosHandler qos_configurator()
{
    ParticipantQosHandler handler("test");
    handler.addUDPV4Transport(32 * 1024 * 1024);
    handler.addSHMTransport(32 * 1024 * 1024);
    handler.setParticipantQosProperties("dsfcversion", "v1.0.0", true);
    return handler;
}

void processHelloWorldOne(const std::string &topic_name, std::shared_ptr<HelloWorldOne> data)
{
    LOG(info) << "recv message [" << topic_name << "]: " << data->index();
}

void test_singale_sub_pub()
{
    std::thread writer_thread(run_dds_data_writer);
    writer_thread.detach();
    std::thread reader_thread(run_dds_data_reader);
    reader_thread.detach();

    while (std::cin.get() != '\n') {
    }
    return;
}

void test_multi_sub_pub(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " sub/pub" << std::endl;
        return;
    }
    if (strcmp(argv[1], "sub") == 0) {
        //run_dds_data_Multireader();
        run_dds_data_reader();
    } else if (strcmp(argv[1], "pub") == 0) {
       //run_dds_data_Multiwriter();
        run_dds_data_writer();
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }
    while (std::cin.get() != '\n') {
    }
    return;
}

void test_singal_node_sub_pub()
{

    DDSParticipantListener listener;
    // DataNode node(170, "test_writer", qos_configurator, &listener);
    DataNode node("/home/wwk/workspaces/test_demo/sample/node_example/qosConfig.xml", &listener);
    node.registerTopicType<HelloWorldOnePubSubType>("wwk");
    auto dataWriter = node.createDataWriter<HelloWorldOne>("wwk");
    auto dataReader = node.createDataReader<HelloWorldOne>("wwk", processHelloWorldOne);
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

    while (std::cin.get() != '\n') {
    }
    return;
}

int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    //test_singale_sub_pub();
    test_multi_sub_pub(argc, argv);
    //test_singal_node_sub_pub();
}

void run_dds_data_writer()
{
    DDSParticipantListener *listener = new DDSParticipantListener();
    DataNode node("/home/wwk/workspaces/test_demo/sample/node_example/qosConfig.xml", listener);
    //DataNode node(170, "test_writer", NULL, listener);
   // DataNode node(0, "test_writer");
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
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void run_dds_data_reader()
{
    DDSParticipantListener *listener = new DDSParticipantListener();
    DataNode node("/home/wwk/workspaces/test_demo/sample/node_example/qosConfig.xml", listener);
    //DataNode node(170, "test_reader", NULL, listener);
    //DataNode node(0, "test_reader");
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
    DataNode node(0, "sender_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    for (const auto& topic : topics) {
        node.registerTopicType<HelloWorldOnePubSubType>(topic);
    }

    // 创建多个数据写入器
    std::unordered_map<std::string, DDSTopicDataWriter<HelloWorldOne> *> dataWriters;

    for (const auto& topic : topics) {
        dataWriters[topic] = node.createDataWriter<HelloWorldOne>(topic);
    }

    std::thread([&]() {
        while (std::cin.get() != '\n') {
        }
        runFlag = false;
    }).detach();

    while (cnt < 100 && runFlag)
    { 
        for (const auto& topic : topics) {
            HelloWorldOne message;
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

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
    DataNode node(1, "receiver_node");

    // 注册多个主题
    std::vector<std::string> topics = {"Topic_1", "Topic_2", "Topic_3"};
    for (const auto& topic : topics) {
        node.registerTopicType<HelloWorldOnePubSubType>(topic);
        // 创建数据读取器
        node.createDataReader<HelloWorldOne>(topic, [](const std::string &topic_name, std::shared_ptr<HelloWorldOne> data) {
            LOG(info) << "recv message from [" << topic_name << "]: " << data->index();
        });
    }

    while (std::cin.get() != '\n') {
    }
    
}
