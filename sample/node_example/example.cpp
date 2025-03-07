// #include <cstring>
// #include <iostream>
// #include <memory>

// #include "fastdds_wrapper/DataNode.h"
// #include "HelloWorldOnePubSubTypes.hpp"
// #include "HelloWorldOne.hpp"
// #include <thread>

// #include "log/logger.h"
// using namespace std;

// void run_dds_data_writer();
// void run_dds_data_reader();

// int main(int argc, char *argv[])
// {
//     Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
//     std::thread writer_thread(run_dds_data_writer);
//     writer_thread.detach();
//     std::thread reader_thread(run_dds_data_reader);
//     reader_thread.detach();
 
//     while (std::cin.get() != '\n') {
//     }
//     return 0;
// }

// void processHelloWorldOne(const std::string &topic_name, std::shared_ptr<HelloWorldOne> data)
// {
//     LOG(info) << "recv message [" << topic_name << "]: " << data->index();
// }



// void run_dds_data_writer()
// {
//     DataNode node(170, "test_writer");
//     node.registerTopicType<HelloWorldOnePubSubType>("1234");
//     auto dataWriter =  node.createDataWriter<HelloWorldOne>("1234");
//     bool runFlag = true;
//     int  index = 0;
//     while (runFlag) {
//         HelloWorldOne message;
//         message.index(++index);
//         message.points(std::vector<uint8_t>(100));
//         if (dataWriter->writeMessage(message)) {
//             LOG(info) << "send message: " << message.index();
//         }
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
// }

// void run_dds_data_reader()
// {   
//     DataNode node(170, "test_reader");
//     node.registerTopicType<HelloWorldOnePubSubType>("1234");
//     auto dataReader =  node.createDataReader<HelloWorldOne>("1234", processHelloWorldOne);   
// }

#include <cstring>
#include <iostream>
#include <memory>

 #include "fastdds_wrapper/DataNode.h"
#include "HelloWorldOnePubSubTypes.hpp"
#include "HelloWorldOne.hpp"
#include <thread>

#include "log/logger.h"
using namespace std;

void run_dds_data_writer();
void run_dds_data_reader();

int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " sub/pub" << std::endl;
        return -1;
    }
    if (strcmp(argv[1], "sub") == 0) {
        run_dds_data_reader();
    } else if (strcmp(argv[1], "pub") == 0) {
        run_dds_data_writer();
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }
    return 0;
}

void processHelloWorldOne(const std::string &topic_name, std::shared_ptr<HelloWorldOne> data)
{
    LOG(info) << "recv message [" << topic_name << "]: " << data->index();
}

ParticipantQosHandler qos_configurator()
{
    ParticipantQosHandler handler("test_writer");
    handler.addUDPV4Transport(32 * 1024 * 1024);
    handler.addSHMTransport(32 * 1024 * 1024);
    handler.setParticipantQosProperties("dsfcversion", "v1.0.0", true);
    return handler;
}

void run_dds_data_writer()
{
    auto ptr = [](){
        ParticipantQosHandler handler("test_writer");
        handler.addUDPV4Transport(32 * 1024 * 1024);
        handler.addSHMTransport(32 * 1024 * 1024);
        handler.setParticipantQosProperties("dsfcversion", "v1.0.0", true);
        return handler;
    };
    
    DataNode node(170, "test_writer", qos_configurator);
    node.registerTopicType<HelloWorldOnePubSubType>("wwk");
    auto dataWriter =  node.createDataWriter<HelloWorldOne>("wwk");
    bool runFlag = true;
    int  index = 0;
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
    DataNode node(170, "test_reader");
    node.registerTopicType<HelloWorldOnePubSubType>("wwk");
    auto dataReader =  node.createDataReader<HelloWorldOne>("wwk", processHelloWorldOne);   
    while (std::cin.get() != '\n') {
    }
}

