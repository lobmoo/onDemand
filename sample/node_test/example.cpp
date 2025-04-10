#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <gtest/gtest.h>
#include "log/logger.h"

// const std::string SEND_FILTER = "UDPTest.SenderTest1k:UDPTest.SenderTest5M:UDPTest.MultiTopicSenderTest1k:UDPTest.MultiTopicSenderTest5M";
// const std::string RECV_FILTER = "UDPTest.ReceiverTest1k:UDPTest.ReceiverTest5M:UDPTest.MultiTopicReceiverTest1k:UDPTest.MultiTopicReceiverTest5M";

const std::string SEND_FILTER = "UDPTest.MultiTopicSenderTest5M";
const std::string RECV_FILTER = "UDPTest.MultiTopicReceiverTest5M";

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    sleep(3);
    std::string mode = argv[1];
    if (mode == "sig") {
        ::testing::GTEST_FLAG(filter) = "UDPTestFixture.*";
    } else if (mode == "mut") {
        ::testing::GTEST_FLAG(filter) = "UDPTestFixtureMult.*";
    } else {
        std::cerr << "Invalid mode. Use 'send' or 'recv'." << std::endl;
        return -1;
    }
    return RUN_ALL_TESTS();
}


