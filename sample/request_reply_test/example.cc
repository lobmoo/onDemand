#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <gtest/gtest.h>
#include "log/logger.h"

// const std::string SEND_FILTER = "UDPTest.SenderTest1k:UDPTest.SenderTest5M:UDPTest.MultiTopicSenderTest1k:UDPTest.MultiTopicSenderTest5M";
// const std::string RECV_FILTER = "UDPTest.ReceiverTest1k:UDPTest.ReceiverTest5M:UDPTest.MultiTopicReceiverTest1k:UDPTest.MultiTopicReceiverTest5M";

volatile int32_t g_testType = 0;

const std::string SEND_FILTER = "UDPTest.MultiTopicSenderTest5M";
const std::string RECV_FILTER = "UDPTest.MultiTopicReceiverTest5M";

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Logger::Instance()->Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    sleep(3);
    std::string mode = argv[1];
    if (mode == "1k") {
        ::testing::GTEST_FLAG(filter) = "UDPTestRequestReply.UDPTestRequestReplyClientTest1k";
        g_testType = 1;
    }
    else if(mode == "100k")
    {
        ::testing::GTEST_FLAG(filter) = "UDPTestRequestReply.UDPTestRequestReplyClientTest100k";
        g_testType = 2;
    }
    else if(mode == "1M")
    {
        ::testing::GTEST_FLAG(filter) = "UDPTestRequestReply.UDPTestRequestReplyClientTest1M";
        g_testType = 3;
    }
    else
    {
        std::cerr << "Invalid mode. Use '1k', '100k', '1M' " << std::endl;
        return 0;
    }
    return RUN_ALL_TESTS();
}


