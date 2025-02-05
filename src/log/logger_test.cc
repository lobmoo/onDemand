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
    Logger::Instance().Init("test.log", Logger::both, Logger::info, 10, 5, false);
  }

  // void TearDown() override {
  //     // 清理日志系统
  //     Logger::Instance().~Logger();
  // }
};

// 功能测试
TEST_F(LoggerTest, FunctionalTest) {
  // 测试不同日志级别
  LOG_INFO << "This is an info message";
  LOG_WARNING << "This is a warning message";
  LOG_ERROR << "This is an error message";

  // 验证日志是否正确写入文件
  std::ifstream logFile("test.log");
  std::string content((std::istreambuf_iterator<char>(logFile)), std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("This is an info message"), std::string::npos);
  EXPECT_NE(content.find("This is a warning message"), std::string::npos);
  EXPECT_NE(content.find("This is an error message"), std::string::npos);
}

// 安全性测试
TEST_F(LoggerTest, SafetyTest) {
  // 测试异常输入
  Logger::Instance().setConsoleLogLevel(Logger::info);
  Logger::Instance().setFileLogLevel(Logger::info);

  // 验证程序不会崩溃
  LOG_INFO << "This is a test message";
  LOG(info) << "This is a test message";
}

// 多线程测试
TEST_F(LoggerTest, MultiThreadTest) {
  const int threadCount = 10;
  const int logCountPerThread = 100;

  std::vector<std::thread> threads;
  for (int i = 0; i < threadCount; ++i) {
    threads.emplace_back([logCountPerThread]() {
      for (int j = 0; j < logCountPerThread; ++j) {
        LOG_INFO << "Log message from thread " << std::this_thread::get_id() << " - " << j;

      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
  // 验证日志文件内容
  std::ifstream logFile("test.log");
  std::string content((std::istreambuf_iterator<char>(logFile)), std::istreambuf_iterator<char>());
  for (int i = 0; i < threadCount * logCountPerThread; ++i) {
    EXPECT_NE(content.find("Log message from thread"), std::string::npos);
  }
}

// 性能测试
TEST_F(LoggerTest, PerformanceTest) {
  const int logCount = 10000;
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < logCount; ++i) {
    LOG_INFO << "Performance test log message " << i;
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