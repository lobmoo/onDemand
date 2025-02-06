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
    using namespace wlog;
    if (!logger::get().init("logs/test.log")) {
        return 1;
    }
    logger::get().set_level(spdlog::level::trace);
  }

  // void TearDown() override {
  //     // 清理日志系统
  //     Logger::Instance().~Logger();
  // }
};


// 性能测试
TEST_F(LoggerTest, PerformanceTest) {
  const int logCount = 10000;
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < logCount; ++i) {
    STREAM_DEBUG << "Performance test log message " << i;
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