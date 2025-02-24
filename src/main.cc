#include <unistd.h>

#include <iostream>
#include <thread>
#include <vector>

#include "logger.h"

void logWorker(int id) {
  for (int i = 0; i < 10000; ++i) {
    LOG(trace) << "Thread " << id << " log message " << i;
    LOG(debug) << "Thread " << id << " log message " << i;
    LOG(info) << "Thread " << id << " log message " << i;
    LOG(warning) << "Thread " << id << " log message " << i;
    LOG(error) << "Thread " << id << " log message " << i;
    LOG(critical) << "Thread " << id << " log message " << i;
  }
}

int main() {
  Logger::Instance().Init("log/myapp.log", Logger::both, Logger::trace, 1, 3);
  std::vector<std::thread> threads;
  // 创建多个线程同时调用 LOG_TIME
  for (int i = 0; i < 1; ++i) {
    threads.emplace_back(logWorker, i);
  }
  for (auto& t : threads) {
    t.join();
  }

  return 0;
}
