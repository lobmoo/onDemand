#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>
#include "logger.h"



// 测试日志单条输出
TEST(LoggerTest, SingleLogOutput) {
    Logger *logger = Logger::Instance();
    ASSERT_TRUE(logger->Init("test.log", Logger::both, Logger::trace, 10, 2));

    LOG(info) << "Hello Logger!";
    // 暂停一下，确保日志刷入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream logFile("test.log");
    ASSERT_TRUE(logFile.is_open());

    std::string line;
    bool found = false;
    while (std::getline(logFile, line)) {
        if (line.find("Hello Logger!") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// 测试大量日志输出单线程
TEST(LoggerTest, MultiLogSingleThread) {
    Logger *logger = Logger::Instance();
    ASSERT_TRUE(logger->Init("test_multi.log", Logger::both, Logger::trace, 100, 3));

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000; ++i) {
        LOG(debug) << "Log entry #" << i;
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Single-thread 10000 logs took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待异步日志 flush

    std::ifstream logFile("test_multi.log");
    ASSERT_TRUE(logFile.is_open());

    size_t count = 0;
    std::string line;
    while (std::getline(logFile, line)) {
        if (!line.empty()) ++count;
    }
    EXPECT_EQ(count, 10000);
}

// 测试多线程日志写入正确性
TEST(LoggerTest, MultiThreadLog) {
    Logger *logger = Logger::Instance();
    ASSERT_TRUE(logger->Init("test_thread.log", Logger::both, Logger::trace, 100, 3, true));

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    const int threadCount = 4;
    const int logsPerThread = 2500; // 4*2500=10000
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([t, logsPerThread]() {
            for (int i = 0; i < logsPerThread; ++i) {
                LOG(debug) << "Thread #" << t << " log #" << i;
            }
        });
    }
    for (auto &th : threads) th.join();

    auto end = std::chrono::steady_clock::now();
    std::cout << "Multi-thread 10000 logs took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 等待异步日志 flush

    std::ifstream logFile("test_thread.log");
    ASSERT_TRUE(logFile.is_open());

    size_t count = 0;
    std::string line;
    while (std::getline(logFile, line)) {
        if (!line.empty()) ++count;
    }
    EXPECT_EQ(count, 10000);
}

// 测试开启异步日志单线程输出正确性
TEST(LoggerTest, AsyncLogSingleThread) {
    Logger *logger = Logger::Instance();
    ASSERT_TRUE(logger->Init("test_async_single.log", Logger::both, Logger::trace, 100, 3, true));

    for (int i = 0; i < 10000; ++i) {
        LOG(debug) << "Async single log #" << i;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::ifstream logFile("test_async_single.log");
    ASSERT_TRUE(logFile.is_open());

    size_t count = 0;
    std::string line;
    while (std::getline(logFile, line)) {
        if (!line.empty()) ++count;
    }
    EXPECT_EQ(count, 10000);
}

// 测试开启异步日志多线程输出正确性
TEST(LoggerTest, AsyncLogMultiThread) {
    Logger *logger = Logger::Instance();
    ASSERT_TRUE(logger->Init("test_async_multi.log", Logger::both, Logger::trace, 100, 3, true));

    const int threadCount = 4;
    const int logsPerThread = 2500;

    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([t, logsPerThread]() {
            for (int i = 0; i < logsPerThread; ++i) {
                LOG(debug) << "Async thread #" << t << " log #" << i;
            }
        });
    }
    for (auto &th : threads) th.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::ifstream logFile("test_async_multi.log");
    ASSERT_TRUE(logFile.is_open());

    size_t count = 0;
    std::string line;
    while (std::getline(logFile, line)) {
        if (!line.empty()) ++count;
    }
    EXPECT_EQ(count, 10000);
}
