#include "logger.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

// 定义测试类
class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 初始化日志系统
    Logger::Instance().Init("log/myapp.log", Logger::both, 0, 1, 3); 
  }

  void TearDown() override {
      // 清理日志系统
      Logger::Instance().Uinit();
  }
};

// TEST_F(LoggerTest, MultiThreadPerformanceTest) {
//   const int threadCount = 100;  // 线程数量
//   const int logPerThread = 1000; // 每个线程写入的日志数，总共 10000 条日志
//   std::vector<std::thread> threads;

//   auto start = std::chrono::high_resolution_clock::now();

//   for (int i = 0; i < threadCount; ++i) {
//     threads.emplace_back([i, logPerThread]() {
//       for (int j = 0; j < logPerThread; ++j) {
//         LOG(info) << "Thread " << i << " log message " << j;
//       }
//     });
//   }

//   for (auto& t : threads) {
//     t.join();
//   }

//   auto end = std::chrono::high_resolution_clock::now();
//   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

//   std::cout << "Logging " << (threadCount * logPerThread) 
//             << " messages using " << threadCount 
//             << " threads took " << duration << " ms" << std::endl;

//   EXPECT_LT(duration, 5000);  // 期望所有日志在 5 秒内完成
// }

// 性能测试
TEST_F(LoggerTest, PerformanceTest) {
  const int logCount = 10000;
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < logCount; ++i) {
    LOG(info) << "Performance test log message " << i;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << "Logging " << logCount << " messages took " << duration << " ms" << std::endl;
  EXPECT_LT(duration, 5000);  // 假设10000条日志记录在1秒内完成
}


TEST(LoggerSingletonTest, SingleInstance) {
    Logger& logger1 = Logger::Instance();
    Logger& logger2 = Logger::Instance();

    // 验证获取的两个 Logger 对象的地址是否相同
    EXPECT_EQ(&logger1, &logger2);
}


const int logCount = 10;  // 1万条日志
const int threadCount = 4;   // 4个线程

// **单线程 LOG_TIME 宏测试**
TEST_F(LoggerTest, SingleThreadPerformanceTest_LOG_TIME) {
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < logCount; ++i) {
    LOG_TIME(info, 100) << "Performance test log message with LOG_TIME " << i;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << "[LOG_TIME] Single-thread logging " << logCount << " messages took " << duration << " ms" << std::endl;
  EXPECT_LT(duration, 5000);
}

TEST_F(LoggerTest, SingleThreadPerformanceTest_LOG) {
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < logCount; ++i) {
    LOG(info) << "Performance test log message with LOG_TIME " << i;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << "[LOG] Single-thread logging " << logCount << " messages took " << duration << " ms" << std::endl;
  EXPECT_LT(duration, 5000);
}



// **多线程性能测试**
void logWorker(int id) {
  for (int i = 0; i < logCount / threadCount; ++i) {
    LOG(info) << "Thread " << id << " log message " << i;
  }
}

TEST_F(LoggerTest, MultiThreadPerformanceTest) {
  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::thread> threads;

  for (int i = 0; i < threadCount; ++i) {
    threads.emplace_back(logWorker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << "[LOG] Multi-thread logging took " << duration << " ms" << std::endl;
  EXPECT_LT(duration, 5000);
}

// **多线程 LOG_TIME 宏测试**
void logWorker_LOG_TIME(int id) {
  for (int i = 0; i < logCount / threadCount; ++i) {
    LOG_TIME(info, 100) << "Thread " << id << " log message " << i;
  }
}

TEST_F(LoggerTest, MultiThreadPerformanceTest_LOG_TIME) {
  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::thread> threads;

  for (int i = 0; i < threadCount; ++i) {
    threads.emplace_back(logWorker_LOG_TIME, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << "[LOG_TIME] Multi-thread logging took " << duration << " ms" << std::endl;
  EXPECT_LT(duration, 5000);
}



int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
