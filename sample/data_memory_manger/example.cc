#include "variable_store.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <unordered_map>

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

// 方式1: 直接使用id (最快)
void test_method1_direct_id(uint32_t test_points, int test_seconds)
{
    std::cout << "\n========== 方式1: 直接使用 id (最快) ==========\n" << std::endl;

    VarStore store;
    std::cout << "注册 " << test_points << " 个点位..." << std::endl;
    for (uint32_t i = 0; i < test_points; ++i) {
        store.register_var(i, sizeof(uint32_t));
    }
    if (!store.finalize()) {
        std::cerr << "Failed to finalize store" << std::endl;
        return;
    }
    std::cout << "初始化完成: " << store.var_count()
              << " 个点位, arena: " << store.total_size() / 1024 << " KB\n"
              << std::endl;

    std::atomic<bool> stop{false};

    // Writer 线程
    std::thread writer([&]() {
        pthread_setname_np(pthread_self(), "proc_id_writer");
        uint32_t cycle = 0;
        while (!stop) {
            auto start = std::chrono::high_resolution_clock::now();
            for (uint32_t i = 0; i < test_points; ++i) {
                uint32_t value = i + cycle * 100;
                store.write(i, &value); // ✅ 直接用i当id
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;

            cycle++;
            if (cycle % 3 == 0) {
                std::cout << "[Writer] 写入 " << test_points << " 个点位耗时: " << std::fixed
                          << std::setprecision(2) << duration.count() << " ms" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Reader 线程
    std::thread reader([&]() {
        pthread_setname_np(pthread_self(), "proc_id_reader");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        while (!stop) {
            auto start = std::chrono::high_resolution_clock::now();
            uint32_t read_errors = 0;
            for (uint32_t i = 0; i < test_points; ++i) {
                uint32_t value;
                if (!store.read(i, &value)) { // ✅ 直接用i当id
                    read_errors++;
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            std::cout << "[Reader] 读取 " << test_points << " 个点位耗时: " << std::fixed
                      << std::setprecision(2) << duration.count() << " ms"
                      << ", 读取错误数: " << read_errors << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // Publisher 线程
    std::thread publisher([&]() {
        pthread_setname_np(pthread_self(), "proc_id_dirty");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        while (!stop) {
            auto start = std::chrono::high_resolution_clock::now();
            uint32_t dirty_count = 0;

            store.for_each_dirty(
                [&](uint32_t id) {
                    auto ptr = store.read_ptr(id);
                    if (ptr) {
                        const uint32_t *value = static_cast<const uint32_t *>(ptr.get());
                        (void)(*value);
                        dirty_count++;
                    }
                },
                test_points);

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;

            std::cout << "[Publisher] 发布脏点 " << dirty_count << " 个, 耗时: " << std::fixed
                      << std::setprecision(2) << duration.count() << " ms"
                      << ", 剩余脏队列: " << store.queue_size_approx() << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(test_seconds));
    stop = true;
    writer.join();
    reader.join();
    publisher.join();
}

// 方式2: 每次都算string+hash+get_id (最慢)
void test_method2_string_hash_every_time(uint32_t test_points, int test_seconds)
{
    std::cout << "\n========== 方式2: 每次 string+hash+get_id (最慢) ==========\n" << std::endl;

    VarStore store;
    std::vector<std::string> id_to_name;
    id_to_name.reserve(test_points);

    std::cout << "注册 " << test_points << " 个点位..." << std::endl;
    for (uint32_t i = 0; i < test_points; ++i) {
        std::string var_name = std::string("var_") + std::to_string(i);
        uint64_t hash = fast_hash(var_name.c_str());
        uint32_t id = store.register_var(hash, sizeof(uint32_t));
        if (id >= id_to_name.size())
            id_to_name.resize(id + 1);
        id_to_name[id] = var_name;
    }
    if (!store.finalize()) {
        std::cerr << "Failed to finalize store" << std::endl;
        return;
    }
    std::cout << "初始化完成: " << store.var_count()
              << " 个点位, arena: " << store.total_size() / 1024 << " KB\n"
              << std::endl;

    std::atomic<bool> stop{false};

    // Writer 线程
    std::thread writer([&]() {
        pthread_setname_np(pthread_self(), "proc_hash_writer");
        uint32_t cycle = 0;
        while (!stop) {

            auto start = std::chrono::high_resolution_clock::now();
            for (uint32_t i = 0; i < test_points; ++i) {
                uint32_t value = i + cycle * 100;
                // ⚠️ 每次都要构造字符串 + 计算hash + get_id
                std::string var_name = std::string("var_") + std::to_string(i);
                uint64_t hash = fast_hash(var_name.c_str());
                uint32_t id = store.get_id(hash);
                store.write(id, &value);
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;

            cycle++;
            if (cycle % 3 == 0) {
                std::cout << "[Writer] 写入 " << test_points << " 个点位耗时: " << std::fixed
                          << std::setprecision(2) << duration.count() << " ms" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Reader 线程
    std::thread reader([&]() {
        pthread_setname_np(pthread_self(), "proc_hash_reader");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        while (!stop) {

            auto start = std::chrono::high_resolution_clock::now();
            uint32_t read_errors = 0;
            for (uint32_t i = 0; i < test_points; ++i) {
                uint32_t value;
                // ⚠️ 每次都要构造字符串 + 计算hash + get_id
                std::string var_name = std::string("var_") + std::to_string(i);
                uint64_t hash = fast_hash(var_name.c_str());
                uint32_t id = store.get_id(hash);
                if (!store.read(id, &value)) {
                    read_errors++;
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            std::cout << "[Reader] 读取 " << test_points << " 个点位耗时: " << std::fixed
                      << std::setprecision(2) << duration.count() << " ms"
                      << ", 读取错误数: " << read_errors << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // Publisher 线程
    std::thread publisher([&]() {
        pthread_setname_np(pthread_self(), "proc_hash_dirty");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        while (!stop) {
            auto start = std::chrono::high_resolution_clock::now();
            uint32_t dirty_count = 0;

            store.for_each_dirty(
                [&](uint32_t id) {
                    const std::string &var_name = id_to_name[id];
                    auto ptr = store.read_ptr(id);
                    if (ptr) {
                        const uint32_t *value = static_cast<const uint32_t *>(ptr.get());
                        (void)(*value);
                        (void)var_name;
                        dirty_count++;
                    }
                },
                test_points);

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;

            std::cout << "[Publisher] 发布脏点 " << dirty_count << " 个, 耗时: " << std::fixed
                      << std::setprecision(2) << duration.count() << " ms"
                      << ", 剩余脏队列: " << store.queue_size_approx() << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(test_seconds));
    stop = true;
    writer.join();
    reader.join();
    publisher.join();
}

// 方式3: 缓存name→id映射 (推荐)
void test_method3_cache_mapping(uint32_t test_points, int test_seconds)
{
    std::cout << "\n========== 方式3: 缓存 name→id 映射 (推荐) ==========\n" << std::endl;

    VarStore store;
    std::vector<std::string> id_to_name;
    std::unordered_map<std::string, uint32_t> name_to_id;

    std::cout << "注册 " << test_points << " 个点位..." << std::endl;
    for (uint32_t i = 0; i < test_points; ++i) {
        std::string var_name = std::string("var_") + std::to_string(i);
        uint64_t hash = fast_hash(var_name.c_str());
        uint32_t id = store.register_var(hash, sizeof(uint32_t));
        if (id >= id_to_name.size())
            id_to_name.resize(id + 1);
        id_to_name[id] = var_name;
        name_to_id[var_name] = id;
    }
    if (!store.finalize()) {
        std::cerr << "Failed to finalize store" << std::endl;
        return;
    }
    std::cout << "初始化完成: " << store.var_count()
              << " 个点位, arena: " << store.total_size() / 1024 << " KB (缓存了映射)\n"
              << std::endl;

    std::atomic<bool> stop{false};

    // Writer 线程
    std::thread writer([&]() {
        pthread_setname_np(pthread_self(), "proc_string_writer");
        uint32_t cycle = 0;
        while (!stop) {
            auto start = std::chrono::high_resolution_clock::now();
            for (uint32_t i = 0; i < test_points; ++i) {
                uint32_t value = i + cycle * 100;
                std::string var_name = std::string("var_") + std::to_string(i);
                uint32_t id = name_to_id[var_name];
                store.write(id, &value);
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;

            cycle++;
            if (cycle % 3 == 0) {
                std::cout << "[Writer] 写入 " << test_points << " 个点位耗时: " << std::fixed
                          << std::setprecision(2) << duration.count() << " ms" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Reader 线程
    std::thread reader([&]() {
        pthread_setname_np(pthread_self(), "proc_string_reader");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        while (!stop) {
            auto start = std::chrono::high_resolution_clock::now();
            uint32_t read_errors = 0;
            for (uint32_t i = 0; i < test_points; ++i) {
                uint32_t value;
                std::string var_name = std::string("var_") + std::to_string(i);
                uint32_t id = name_to_id[var_name]; // ✅ 查映射表
                if (!store.read(id, &value)) {
                    read_errors++;
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            std::cout << "[Reader] 读取 " << test_points << " 个点位耗时: " << std::fixed
                      << std::setprecision(2) << duration.count() << " ms"
                      << ", 读取错误数: " << read_errors << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // Publisher 线程
    std::thread publisher([&]() {
        pthread_setname_np(pthread_self(), "proc_string_dirty");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        while (!stop) {

            auto start = std::chrono::high_resolution_clock::now();
            uint32_t dirty_count = 0;

            store.for_each_dirty(
                [&](uint32_t id) {
                    const std::string &var_name = id_to_name[id];
                    auto ptr = store.read_ptr(id);
                    if (ptr) {
                        const uint32_t *value = static_cast<const uint32_t *>(ptr.get());
                        (void)(*value);
                        (void)var_name;
                        dirty_count++;
                    }
                },
                test_points);

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;

            std::cout << "[Publisher] 发布脏点 " << dirty_count << " 个, 耗时: " << std::fixed
                      << std::setprecision(2) << duration.count() << " ms"
                      << ", 剩余脏队列: " << store.queue_size_approx() << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(test_seconds));
    stop = true;
    writer.join();
    reader.join();
    publisher.join();
}

int main(int argc, char *argv[])
{
    uint32_t test_points = 1000000;
    int test_seconds = 5;
    int test_method = 0;

    if (argc > 1)
        test_method = std::atoi(argv[1]);
    if (argc > 2)
        test_points = std::atoi(argv[2]);
    if (argc > 3)
        test_seconds = std::atoi(argv[3]);

    std::cout << "\n========================================" << std::endl;
    std::cout << "性能对比测试" << std::endl;
    std::cout << "测试点位数: " << test_points << std::endl;
    std::cout << "每个测试时长: " << test_seconds << " 秒" << std::endl;
    std::cout << "========================================" << std::endl;

    if (test_method == 0 || test_method == 1) {
        test_method1_direct_id(test_points, test_seconds);
        if (test_method == 0)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (test_method == 0 || test_method == 2) {
        test_method2_string_hash_every_time(test_points, test_seconds);
        if (test_method == 0)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (test_method == 0 || test_method == 3) {
        test_method3_cache_mapping(test_points, test_seconds);
    }

    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "测试完成!" << std::endl;
    std::cout << "\n使用方法:" << std::endl;
    std::cout << "  ./performance_test [方式] [点位数] [秒数]" << std::endl;
    std::cout << "  方式: 0=全部, 1=直接id, 2=每次hash, 3=缓存映射" << std::endl;
    std::cout << "  示例: ./performance_test 1 10000 5" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return 0;
}
