#include <unistd.h>
#include <iostream>
#include <thread>
#include <vector>
#include "logger.h"



void logTask() {
    for (int i = 0; i < 10000000; ++i) {
        LOG_TIME(info, 1000) << "Test log from thread " << std::this_thread::get_id() << " with i = " << i;
        //LOG(info) << "Test log from thread " << std::this_thread::get_id() << " with i = " << i;
    }
}

int main() {
    Logger::Instance().Init("log/myapp.log", Logger::both, 0, 1, 3); 
    std::vector<std::thread> threads;
    

    // 创建多个线程同时调用 LOG_TIME
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(logTask);
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}

    
