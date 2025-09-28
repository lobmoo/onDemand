#include <iostream>
#include <string>
#include <vector>
#include "fifo.h"
#include "log/logger.h"
#include "timer_wheel/thread_pool.h"
#include "timer_wheel/timer_scheduler.h"
#include <spdlog/async.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <numeric>

#include <spdlog/spdlog.h>




void testNativeSpdlogAsync() {
    std::cout << "=== Native spdlog Async Test ===" << std::endl;
    
    const int kLogCount = 100000;
    
    // 直接使用spdlog的异步功能
    spdlog::init_thread_pool(8192, std::thread::hardware_concurrency() / 4);  // 8KB队列，1个工作线程
    
    // 同步测试
    auto sync_logger = spdlog::basic_logger_mt("sync_logger", "spdlog_sync.log");
    sync_logger->set_level(spdlog::level::info);
    
    auto sync_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kLogCount; ++i) {
        sync_logger->info("Native sync message {}", i);
    }
    auto sync_end = std::chrono::high_resolution_clock::now();
    sync_logger->flush();
    spdlog::drop("sync_logger");
    
    // 异步测试
    auto async_logger = spdlog::basic_logger_mt<spdlog::async_factory>("async_logger", "spdlog_async.log");
    async_logger->set_level(spdlog::level::info);
    
    auto async_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kLogCount; ++i) {
        async_logger->info("Native async message {}", i);
    }
    auto async_submit_end = std::chrono::high_resolution_clock::now();
    
    // 等待异步完成
    spdlog::shutdown();  // 这会等待所有异步日志完成
    auto async_complete_end = std::chrono::high_resolution_clock::now();
    
    auto sync_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sync_end - sync_start);
    auto async_submit_duration = std::chrono::duration_cast<std::chrono::milliseconds>(async_submit_end - async_start);
    auto async_complete_duration = std::chrono::duration_cast<std::chrono::milliseconds>(async_complete_end - async_start);
    
    std::cout << "Native spdlog results:" << std::endl;
    std::cout << "Sync:           " << sync_duration.count() << "ms" << std::endl;
    std::cout << "Async submit:   " << async_submit_duration.count() << "ms" << std::endl;
    std::cout << "Async complete: " << async_complete_duration.count() << "ms" << std::endl;
    std::cout << "Submit speedup: " << (double)sync_duration.count() / async_submit_duration.count() << "x" << std::endl;
}


void testMyLoggerAsync() {
    std::cout << "=== My Logger Async Test ===" << std::endl;
    
    const int kLogCount = 100000;
    
    // 同步测试 - 使用我的Logger
    Logger::GetInstance()->Init("my_sync.log", Logger::file, Logger::info, 60, 3, false);
    
    auto sync_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kLogCount; ++i) {
        LOG(info) << "My sync message " << i;
    }
    auto sync_end = std::chrono::high_resolution_clock::now();
    Logger::GetInstance()->Uninit();
    
    // 异步测试 - 使用我的Logger
    Logger::GetInstance()->Init("my_async.log", Logger::file, Logger::info, 60, 3, true);
    
    auto async_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kLogCount; ++i) {
        LOG(info) << "My async message " << i;
    }
    auto async_submit_end = std::chrono::high_resolution_clock::now();
    
    // 等待异步完成
    Logger::GetInstance()->Uninit();  // 这会等待所有异步日志完成
    auto async_complete_end = std::chrono::high_resolution_clock::now();
    
    auto sync_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sync_end - sync_start);
    auto async_submit_duration = std::chrono::duration_cast<std::chrono::milliseconds>(async_submit_end - async_start);
    auto async_complete_duration = std::chrono::duration_cast<std::chrono::milliseconds>(async_complete_end - async_start);
    
    std::cout << "My Logger results:" << std::endl;
    std::cout << "Sync:           " << sync_duration.count() << "ms" << std::endl;
    std::cout << "Async submit:   " << async_submit_duration.count() << "ms" << std::endl;
    std::cout << "Async complete: " << async_complete_duration.count() << "ms" << std::endl;
    std::cout << "Submit speedup: " << (double)sync_duration.count() / async_submit_duration.count() << "x" << std::endl;
}

void testNativeSpdlogSingleCallTime() {
    std::cout << "=== Native spdlog Single Call Time Test ===" << std::endl;
    
    // 预热和准备
    spdlog::init_thread_pool(8192, std::thread::hardware_concurrency() / 4);
    
    // 测试同步单次调用
    auto sync_logger = spdlog::basic_logger_mt("sync_single", "native_sync_single.log");
    sync_logger->set_level(spdlog::level::info);
    
    std::vector<long> sync_times;
    sync_times.reserve(100);
    
    // 预热几次调用
    for (int i = 0; i < 10; ++i) {
        sync_logger->info("Warmup message {}", i);
    }
    
    // 测量100次单独调用
    for (int i = 0; i < 100; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        sync_logger->info("Native sync single call {}", i);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        sync_times.push_back(duration.count());
    }
    
    sync_logger->flush();
    spdlog::drop("sync_single");
    
    // 测试异步单次调用
    auto async_logger = spdlog::basic_logger_mt<spdlog::async_factory>("async_single", "native_async_single.log");
    async_logger->set_level(spdlog::level::info);
    
    std::vector<long> async_times;
    async_times.reserve(100);
    
    // 预热几次调用
    for (int i = 0; i < 10; ++i) {
        async_logger->info("Warmup message {}", i);
    }
    
    // 测量100次单独调用
    for (int i = 0; i < 100; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        async_logger->info("Native async single call {}", i);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        async_times.push_back(duration.count());
    }
    
    spdlog::shutdown();
    
    // 计算统计数据
    auto calcStats = [](const std::vector<long>& times) {
        long sum = std::accumulate(times.begin(), times.end(), 0L);
        double avg = (double)sum / times.size();
        
        std::vector<long> sorted_times = times;
        std::sort(sorted_times.begin(), sorted_times.end());
        
        long min_time = sorted_times.front();
        long max_time = sorted_times.back();
        long median = sorted_times[sorted_times.size() / 2];
        
        return std::make_tuple(avg, min_time, max_time, median);
    };
    
    auto [sync_avg, sync_min, sync_max, sync_median] = calcStats(sync_times);
    auto [async_avg, async_min, async_max, async_median] = calcStats(async_times);
    
    std::cout << "Native spdlog single call statistics (100 samples):" << std::endl;
    std::cout << "Sync  - Avg: " << (sync_avg / 1000.0) << "μs, Min: " << (sync_min / 1000.0) 
              << "μs, Max: " << (sync_max / 1000.0) << "μs, Median: " << (sync_median / 1000.0) << "μs" << std::endl;
    std::cout << "Async - Avg: " << (async_avg / 1000.0) << "μs, Min: " << (async_min / 1000.0) 
              << "μs, Max: " << (async_max / 1000.0) << "μs, Median: " << (async_median / 1000.0) << "μs" << std::endl;
    std::cout << "Response improvement: " << (sync_avg / async_avg) << "x" << std::endl;
}

void testMyLoggerSingleCallTime() {
    std::cout << "=== My Logger Single Call Time Test ===" << std::endl;
    
    // 测试同步单次调用
    Logger::GetInstance()->Init("my_sync_single.log", Logger::file, Logger::info, 60, 3, false);
    
    std::vector<long> sync_times;
    sync_times.reserve(100);
    
    // 预热几次调用
    for (int i = 0; i < 10; ++i) {
        LOG(info) << "Warmup message " << i;
    }
    
    // 测量100次单独调用
    for (int i = 0; i < 100; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        LOG(info) << "My sync single call " << i;
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        sync_times.push_back(duration.count());
    }
    
    Logger::GetInstance()->Uninit();
    
    // 测试异步单次调用
    Logger::GetInstance()->Init("my_async_single.log", Logger::file, Logger::info, 60, 3, true);
    
    std::vector<long> async_times;
    async_times.reserve(100);
    
    // 预热几次调用
    for (int i = 0; i < 10; ++i) {
        LOG(info) << "Warmup message " << i;
    }
    
    // 测量100次单独调用
    for (int i = 0; i < 100; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        LOG(info) << "My async single call " << i;
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        async_times.push_back(duration.count());
    }
    
    Logger::GetInstance()->Uninit();
    
    // 计算统计数据
    auto calcStats = [](const std::vector<long>& times) {
        long sum = std::accumulate(times.begin(), times.end(), 0L);
        double avg = (double)sum / times.size();
        
        std::vector<long> sorted_times = times;
        std::sort(sorted_times.begin(), sorted_times.end());
        
        long min_time = sorted_times.front();
        long max_time = sorted_times.back();
        long median = sorted_times[sorted_times.size() / 2];
        
        return std::make_tuple(avg, min_time, max_time, median);
    };
    
    auto [sync_avg, sync_min, sync_max, sync_median] = calcStats(sync_times);
    auto [async_avg, async_min, async_max, async_median] = calcStats(async_times);
    
    std::cout << "My Logger single call statistics (100 samples):" << std::endl;
    std::cout << "Sync  - Avg: " << (sync_avg / 1000.0) << "μs, Min: " << (sync_min / 1000.0) 
              << "μs, Max: " << (sync_max / 1000.0) << "μs, Median: " << (sync_median / 1000.0) << "μs" << std::endl;
    std::cout << "Async - Avg: " << (async_avg / 1000.0) << "μs, Min: " << (async_min / 1000.0) 
              << "μs, Max: " << (async_max / 1000.0) << "μs, Median: " << (async_median / 1000.0) << "μs" << std::endl;
    std::cout << "Response improvement: " << (sync_avg / async_avg) << "x" << std::endl;
}

int main() {
    // testSingleThreadResponseTime();
    // std::cout << "\n" << std::endl;
    
    // testIOIntensiveScenario();
    // std::cout << "\n" << std::endl;
    
    // testTrueMultithreadComparison();
    testNativeSpdlogSingleCallTime();
    testMyLoggerSingleCallTime();
    testNativeSpdlogAsync();
    testMyLoggerAsync();
    return 0;
}