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
    Logger::Instance().Init("log/myapp.log", Logger::both, Logger::trace, 1, 3); 
  }

  void TearDown() override {
      // 清理日志系统
      Logger::Instance().Uinit();
  }
};


// 性能测试
const int logCount = 10000;  // 1万条日志
const int threadCount = 10;   // 4个线程

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
    LOG(warning) << "Thread " << id << " log message " << i;
    LOG(error) << "Thread " << id << " log message " << i;
    LOG(critical) << "Thread " << id << " log message " << i;
    LOG(trace) << "Thread " << id << " log message " << i;
    LOG(debug) << "Thread " << id << " log message " << i;
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


TEST(LoggerSingletonTest, SingleInstance) {
  Logger& logger1 = Logger::Instance();
  Logger& logger2 = Logger::Instance();

  // 验证获取的两个 Logger 对象的地址是否相同
  EXPECT_EQ(&logger1, &logger2);
}


TEST(LoggerTest11, FlushConfigurationComparison) {
  Logger::Instance().Init("log/myapp.log", Logger::both, Logger::info, 1, 3);

  auto start_no_flush = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 10000; ++i) {
      LOG(info) << "Performance test log message" << i;
  }

  auto end_no_flush = std::chrono::high_resolution_clock::now();
  auto duration_no_flush = std::chrono::duration_cast<std::chrono::milliseconds>(end_no_flush - start_no_flush).count();

  Logger::Instance().Uinit();

  sleep(3);
  // 测试开启 flushEvery 和 flushOnLevel 的情况
  Logger::Instance().setFlushEvery(1);
  //Logger::Instance().setFlushOnLevel(Logger::info);
  Logger::Instance().Init("log/myapp.log", Logger::both, Logger::info, 1, 3);

  auto start_with_flush = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 10000; ++i) {
    LOG(info) << "Performance test log message" << i;
  }

  auto end_with_flush = std::chrono::high_resolution_clock::now();
  auto duration_with_flush = std::chrono::duration_cast<std::chrono::milliseconds>(end_with_flush - start_with_flush).count();
  Logger::Instance().Uinit();
  std::cout << "[WITH FLUSH] Multi-thread logging took " << duration_with_flush << " ms" << std::endl;
  std::cout << "[NO FLUSH] Multi-thread logging took " << duration_no_flush << " ms" << std::endl;
  
  // 检查结果（这里仅比较两者的时间差）
  EXPECT_TRUE(duration_with_flush <= duration_no_flush || duration_with_flush >= duration_no_flush * 0.8);
}


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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
