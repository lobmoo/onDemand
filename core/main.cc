#include <unistd.h>

#include <iostream>
#include <thread>
#include <vector>

#include "log/logger.h"

void logWorker(int id) {
  for (int i = 0; i < 10000; ++i) {
    LOG(trace) << "Thread " << id << " log message " << i;
    LOG(debug) << "Thread " << id << " log message " << i;
    LOG(info) << "Thread " << id << " log message " << i;
    LOG(warning) << "Thread " << id << " log message " << i;
    LOG(error) << "Thread " << id << " log message " << i;
    LOG(critical) << "Thread " << id << " log message " << i;
    
    LOG_TIME(trace, 2000) << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++ " << id;
    LOG_TIME(trace, 2000) << "Thread " << id << " log message " << i;
    LOG_TIME(debug, 1000) << "Thread " << id << " log message " << i;
    LOG_TIME(info, 3000) << "Thread " << id << " log message " << i;
    LOG_TIME(warning, 4000) << "Thread " << id << " log message " << i;
    LOG_TIME(error, 6000) << "Thread " << id << " log message " << i;
    LOG_TIME(critical, 3000) << "Thread " << id << " log message " << i;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}





int main() {
  Logger::Instance().setFlushOnLevel(Logger::info);
  Logger::Instance().Init("log/myapp.log", Logger::both, Logger::trace, 60, 5);
  std::vector<std::thread> threads;
  // 创建多个线程同时调用 LOG_TIME
  for (int i = 0; i < 100; ++i) {
    threads.emplace_back(logWorker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  return 0;
}
