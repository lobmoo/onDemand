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

ParticipantQosHandler qos_configurator_subscriber()
{
    ParticipantQosHandler handler("test");
    // try{
    // // 身份认证
    // std::string identity_ca_path = "file:///home/weiqb/src/test_demo/config/certs/maincacert.pem";
    // std::string sub_cert_path = "file:///home/weiqb/src/test_demo/config/certs/subcert.pem";
    // std::string sub_privateKey_path = "file:///home/weiqb/src/test_demo/config/certs/subkey.pem";
    // handler.setAuthenticationPlugin(identity_ca_path, sub_cert_path, sub_privateKey_path);
    // // 访问控制
    // std::string governance_path = "file:///home/weiqb/src/test_demo/config/certs/governance.smime";
    // std::string permission_path = "file:///home/weiqb/src/test_demo/config/certs/permissions.smime";
    // handler.setAccessControlPlugin(identity_ca_path, governance_path, permission_path);
    // // 加密插件 待测试
    // handler.setCryptographicPlugin();
    // // 日志插件
    // std::string log_file_path = "/home/weiqb/src/test_demo/config/certs/logfile/sub.log";   
    // handler.setSecurityLogging("INFORMATIONAL_LEVEL", log_file_path);
    // } catch (std::exception &e) {        std::cout << e.what() << std::endl; }
    return handler;
}

ParticipantQosHandler qos_configurator_publisher()
{
    ParticipantQosHandler handler("test");
    
    // // 身份认证
    // std::string identity_ca_path = "file:///home/weiqb/src/test_demo/config/certs/maincacert.pem";
    // std::string sub_cert_path = "file:///home/weiqb/src/test_demo/config/certs/pubcert.pem";
    // std::string sub_privateKey_path = "file:///home/weiqb/src/test_demo/config/certs/pubkey.pem";
    // handler.setAuthenticationPlugin(identity_ca_path, sub_cert_path, sub_privateKey_path);
    // // 访问控制
    // std::string governance_path = "file:///home/weiqb/src/test_demo/config/certs/governance.smime";
    // std::string permission_path = "file:///home/weiqb/src/test_demo/config/certs/permissions.smime";
    // handler.setAccessControlPlugin(identity_ca_path, governance_path, permission_path);
    // // 加密插件 待测试
    // handler.setCryptographicPlugin();
    // // 日志插件
    // std::string log_file_path = "/home/weiqb/src/test_demo/config/certs/logfile/pub.log";   
    // handler.setSecurityLogging("DEBUG_LEVEL", log_file_path);

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
    // 创建participant
    DDSParticipantListener *listener = new DDSParticipantListener();
    DataNode node(10, "test_writer", qos_configurator_publisher, listener);
    // DataNode node("/home/weiqb/src/test_demo/sample/node_example/qosConfig.xml", listener);
    //DataNode node(0, "test_writer", qos_configurator);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void run_dds_data_reader()
{
    DDSParticipantListener *listener = new DDSParticipantListener();
    DataNode node(10, "test_reader", qos_configurator_subscriber, listener);
    // DataNode node("/home/weiqb/src/test_demo/sample/node_example/qosConfig.xml", listener);
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