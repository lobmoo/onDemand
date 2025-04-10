#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <gtest/gtest.h>
#include "log/logger.h"



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    std::string mode = argv[1];
    if (mode == "send") {
        ::testing::GTEST_FLAG(filter) = "UDPTest.SenderTest1k:UDPTest.SenderTest5M:UDPTest.MultiTopicSenderTest1k:UDPTest.MultiTopicSenderTest5M";
    } else if (mode == "recv") {
        ::testing::GTEST_FLAG(filter) = "UDPTest.ReceiverTest1k:UDPTest.ReceiverTest5M:UDPTest.MultiTopicReceiverTest1k:UDPTest.MultiTopicReceiverTest5M";
    } else {
        std::cerr << "Invalid mode. Use 'send' or 'recv'." << std::endl;
        return -1;
    }
    return RUN_ALL_TESTS();
}

// #include "TestMessage_1k/TestMessage.hpp"
// #include "TestMessage_1k/TestMessagePubSubTypes.hpp"

// #include "TestMessage_100k/TestMessage.hpp"
// #include "TestMessage_100k/TestMessagePubSubTypes.hpp"

// #include "TestMessage_1M/TestMessage.hpp"
// #include "TestMessage_1M/TestMessagePubSubTypes.hpp"

// #include "TestMessage_10M/TestMessage.hpp"
// #include "TestMessage_10M/TestMessagePubSubTypes.hpp"

// #include "TestMessage_100M/TestMessage.hpp"
// #include "TestMessage_100M/TestMessagePubSubTypes.hpp"

// #include "fastdds_wrapper/DataNode.h"
// #include "fastdds_wrapper/DDSParticipantListener.h"
// #include "log/logger.h"
// using namespace std;


// void processHelloWorldOne(const std::string &topic_name, std::shared_ptr<Message_512> data)
// {
//     auto now = std::chrono::system_clock::now();
//     auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
//     auto epoch = value.time_since_epoch();
//     auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
//     int64_t delay_time =  timestamp - data->timestamp();
//     LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
// }


// void run_dds_data_writer()
// {
//     DataNode node(170, "test_writer");
//     node.registerTopicType<Message_512PubSubType>("test1");
//     auto dataWriter = node.createDataWriter<Message_512>("test1");
//     bool runFlag = true;
//     int index = 0;
//     std::thread([&]() {
//         while (std::cin.get() != '\n') {
//         }
//         runFlag = false;
//     }).detach();

//     while (runFlag) {
//         Message_512 message;
//         auto now = std::chrono::system_clock::now();
//         auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
//         auto epoch = value.time_since_epoch();
//         auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
//         message.timestamp(timestamp);
//         message.id(index++);
//         std::array<int16_t, 512> data;
        
//         for (size_t i = 0; i < data.size(); ++i) {
//             data[i] = static_cast<int16_t>(i); // Ê¾ÀýÊý¾Ý
//         }
//         message.data(data);
//         if (dataWriter->writeMessage(message)) {
//             LOG(info) << "send message: " << message.id();
//         }
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
// }


// void run_dds_data_reader()
// {
//     DataNode node(170, "test_reader");
//     node.registerTopicType<Message_512PubSubType>("test1");
//     auto dataReader = node.createDataReader<Message_512>("test1", processHelloWorldOne);
//     while (std::cin.get() != '\n') {
//     }
// }


// int main(int argc, char *argv[])
// {
//     Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
//     if (argc < 2) {
//         std::cerr << "Usage: " << argv[0] << " sub/pub" << std::endl;
//         return -1;
//     }
//     if (strcmp(argv[1], "sub") == 0) {
//         run_dds_data_reader();
//     } else if (strcmp(argv[1], "pub") == 0) {
//         run_dds_data_writer();
//     } else {
//         std::cerr << "unknown command: " << argv[1] << std::endl;
//     }
//     while (std::cin.get() != '\n') {
//     }
//     return 0;
// }


