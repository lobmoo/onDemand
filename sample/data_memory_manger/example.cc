#include "variable_store.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <cstring>
#include <iomanip>

using namespace varstore;

inline uint64_t fast_hash(const char *str)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    while (*str) {
        hash ^= static_cast<uint64_t>(*str++);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

int main(int argc, char *argv[])
{
    VarStore store;
    uint32_t initial_points = 100000; // 初始点位数

    // 注册初始点位
    std::cout << "注册 " << initial_points << " 个点位..." << std::endl;
    for (uint32_t i = 0; i < initial_points; ++i) {
        uint64_t hash_value = fast_hash((std::string("var_") + std::to_string(i)).c_str());
        uint32_t id = store.register_var(hash_value, sizeof(uint32_t));
        if (id == VarStore::kInvalidId) {
            std::cerr << "Failed to register var " << i << std::endl;
            return 1;
        }
    }

    if (!store.finalize()) {
        std::cerr << "Failed to finalize store" << std::endl;
        return 1;
    }
    std::cout << "初始化完成: " << store.var_count()
              << " 个点位, arena: " << store.total_size() / 1024 << " KB" << std::endl;
    std::cout << std::endl;

    // Writer 线程
    std::thread t1([&store, initial_points]() {
        uint32_t cycle = 0;
        while (true) {
            auto start = std::chrono::high_resolution_clock::now();
            for (uint32_t i = 0; i < initial_points; ++i) {
                uint32_t value = i + cycle * 100;
                store.write(i, &value);
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            
            cycle++;
            if (cycle % 10 == 0) {
                std::cout << "[Writer] 写入 " << initial_points << " 个点位耗时: "
                          << std::fixed << std::setprecision(2) << duration.count() << " ms" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Reader 线程
    std::thread t2([&store]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        while (true) {
            auto start = std::chrono::high_resolution_clock::now();
            uint32_t read_errors = 0;
            for (uint32_t i = 0; i < store.var_count(); ++i) {
                uint32_t value = 0;
                if (!store.read(i, &value)) {
                    read_errors++;
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            std::cout << "[Reader] 读取 " << store.var_count() << " 个点位耗时: "
                      << std::fixed << std::setprecision(2) << duration.count() << " ms"
                      << ", 读取错误数: " << read_errors << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // ✅ 新增：脏数据发布线程
    std::thread t3([&store]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        while (true) {
            auto start = std::chrono::high_resolution_clock::now();
            uint32_t dirty_count = 0;
            
            store.for_each_dirty([&](uint32_t id) {
                // 模拟读取并发布
                auto ptr = store.read_ptr(id);
                if (ptr) {
                    const uint32_t* value = static_cast<const uint32_t*>(ptr.get());
                    (void)(*value);  // 模拟使用数据
                    dirty_count++;
                }
            }, 50000);  // 每次批量处理5万个
            
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            
            std::cout << "[Publisher] 发布脏点 " << dirty_count << " 个, 耗时: "
                      << std::fixed << std::setprecision(2) << duration.count() << " ms"
                      << ", 剩余脏队列: " << store.queue_size_approx() << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
