#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include "log/logger.h"

int main()
{

    Logger::GetInstance()->Uninit();
    Logger::GetInstance()->Init("log/1.log", Logger::both, Logger::info, 10, 3, true);
    LOG(info) << "This is a test log message.";
    LOG(info) << "This is a test log message.";
    Logger::GetInstance()->Uninit();
    Logger::GetInstance()->Init("log/2.log", Logger::both, Logger::info, 10, 3, true);
    while (1) {
        LOG(info) << "This is a test log message.";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}