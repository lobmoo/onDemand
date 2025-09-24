#include "timer_scheduler.h"
#include <gtest/gtest.h>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>
#include "timer_scheduler.h"

class TimerSchedulerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        scheduler = std::make_unique<TimerScheduler>(10, 4); // 10ms tick, 4 threads
    }

    void TearDown() override { scheduler.reset(); }

    std::unique_ptr<TimerScheduler> scheduler;
};

// 测试单次任务调度
TEST_F(TimerSchedulerTest, SingleTaskScheduling)
{
    std::atomic<int> count{0};

    auto timer = scheduler->schedule([&count]() { count.fetch_add(1, std::memory_order_relaxed); },
                                     10); // 100ms delay (10 ticks)

    EXPECT_TRUE(timer != nullptr);
    EXPECT_TRUE(timer->active());

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(count.load(), 1);
    EXPECT_FALSE(timer->active()); // 单次任务执行后应该不活跃
}

// 测试多个单次任务
TEST_F(TimerSchedulerTest, MultipleSingleTasks)
{
    std::atomic<int> count{0};
    std::vector<std::shared_ptr<TimerEventInterface>> timers;

    // 调度5个任务
    for (int i = 0; i < 5; ++i) {
        auto timer =
            scheduler->schedule([&count]() { count.fetch_add(1, std::memory_order_relaxed); },
                                (i + 1) * 5); // 不同的延迟时间

        timers.push_back(timer);
        EXPECT_TRUE(timer->active());
    }

    // 等待所有任务执行完成
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(count.load(), 5);

    // 检查所有任务都不活跃了
    for (auto &timer : timers) {
        EXPECT_FALSE(timer->active());
    }
}

// 测试周期性任务
TEST_F(TimerSchedulerTest, RecurringTaskScheduling)
{
    std::atomic<int> count{0};

    auto timer =
        scheduler->scheduleRecurring([&count]() { count.fetch_add(1, std::memory_order_relaxed); },
                                     5, 10); // 50ms初始延迟，100ms间隔

    EXPECT_TRUE(timer != nullptr);
    EXPECT_TRUE(timer->active());

    // 等待足够长时间让任务执行多次
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    // 应该执行至少3次 (50ms + 100ms + 100ms + 100ms = 350ms)
    int final_count = count.load();
    EXPECT_GE(final_count, 3);
    EXPECT_LE(final_count, 4);    // 考虑到时间误差，不应该超过4次
    EXPECT_TRUE(timer->active()); // 周期性任务应该仍然活跃
}

// 测试任务取消
TEST_F(TimerSchedulerTest, TaskCancellation)
{
    std::atomic<int> count{0};

    auto timer = scheduler->schedule([&count]() { count.fetch_add(1, std::memory_order_relaxed); },
                                     20); // 200ms delay

    EXPECT_TRUE(timer->active());

    // 在任务执行前取消
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    scheduler->cancel(timer);

    EXPECT_FALSE(timer->active());

    // 等待原本应该执行的时间
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(count.load(), 0); // 任务应该没有执行
}

// 测试周期性任务取消
TEST_F(TimerSchedulerTest, RecurringTaskCancellation)
{
    std::atomic<int> count{0};

    auto timer =
        scheduler->scheduleRecurring([&count]() { count.fetch_add(1, std::memory_order_relaxed); },
                                     5, 10); // 50ms初始延迟，100ms间隔

    EXPECT_TRUE(timer->active());

    // 让任务执行几次
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int count_before_cancel = count.load();
    EXPECT_GE(count_before_cancel, 1);

    // 取消任务
    scheduler->cancel(timer);
    EXPECT_FALSE(timer->active());

    // 等待一段时间，确保任务不再执行
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(count.load(), count_before_cancel); // 计数不应该增加
}

// 测试立即执行任务 (post)
TEST_F(TimerSchedulerTest, ImmediateTaskExecution)
{
    std::atomic<int> count{0};

    scheduler->post([&count]() { count.fetch_add(1, std::memory_order_relaxed); });

    // 等待短时间让任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(count.load(), 1);
}

// 测试多个立即执行任务
TEST_F(TimerSchedulerTest, MultipleImmediateTasks)
{
    std::atomic<int> count{0};
    const int num_tasks = 10;

    for (int i = 0; i < num_tasks; ++i) {
        scheduler->post([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
    }

    // 等待所有任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(count.load(), num_tasks);
}

// 测试任务执行顺序
TEST_F(TimerSchedulerTest, TaskExecutionOrder)
{
    std::vector<int> execution_order;
    std::mutex order_mutex;

    // 调度不同延迟的任务
    scheduler->schedule(
        [&execution_order, &order_mutex]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(3);
        },
        15); // 150ms

    scheduler->schedule(
        [&execution_order, &order_mutex]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(1);
        },
        5); // 50ms

    scheduler->schedule(
        [&execution_order, &order_mutex]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(2);
        },
        10); // 100ms

    // 等待所有任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 检查执行顺序
    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 1);
    EXPECT_EQ(execution_order[1], 2);
    EXPECT_EQ(execution_order[2], 3);
}

// 测试异常处理
TEST_F(TimerSchedulerTest, ExceptionHandling)
{
    std::atomic<int> count{0};

    // 调度一个会抛出异常的任务
    auto timer1 = scheduler->schedule(
        [&count]() {
            count.fetch_add(1, std::memory_order_relaxed);
            throw std::runtime_error("Test exception");
        },
        5);

    // 调度一个正常的任务
    auto timer2 =
        scheduler->schedule([&count]() { count.fetch_add(1, std::memory_order_relaxed); }, 10);

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 两个任务都应该被调用，异常不应该影响其他任务
    EXPECT_EQ(count.load(), 2);
}

// 测试并发安全性
TEST_F(TimerSchedulerTest, ConcurrentSafety)
{
    std::atomic<int> count{0};
    const int num_tasks = 100;

    // 从多个线程同时调度任务
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < num_tasks / 4; ++i) {
                auto timer = scheduler->schedule(
                    [&count]() { count.fetch_add(1, std::memory_order_relaxed); },
                    (t * 10) + (i % 10) + 1);

                // 不需要保存timer引用，只关心任务是否执行
                // 避免了复杂的同步问题
            }
        });
    }

    // 等待所有线程完成调度
    for (auto &thread : threads) {
        thread.join();
    }

    // 等待所有任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(count.load(), num_tasks);
}

// 测试定时器销毁时的清理
TEST_F(TimerSchedulerTest, TimerDestructionCleanup)
{
    std::atomic<int> count{0};

    {
        TimerScheduler temp_scheduler(10, 2);

        auto timer = temp_scheduler.schedule(
            [&count]() { count.fetch_add(1, std::memory_order_relaxed); }, 20); // 200ms delay

        EXPECT_TRUE(timer->active());

        // temp_scheduler 在这里销毁
    }

    // 等待原本应该执行的时间
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 任务不应该执行，因为调度器已经销毁
    EXPECT_EQ(count.load(), 0);
}

// 性能测试
TEST_F(TimerSchedulerTest, PerformanceTest)
{
    const int num_tasks = 1000;
    std::atomic<int> count{0};

    auto start = std::chrono::high_resolution_clock::now();

    // 调度大量任务
    for (int i = 0; i < num_tasks; ++i) {
        scheduler->schedule([&count]() { count.fetch_add(1, std::memory_order_relaxed); },
                            (i % 100) + 1);
    }

    auto schedule_end = std::chrono::high_resolution_clock::now();

    // 等待所有任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(count.load(), num_tasks);

    auto schedule_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(schedule_end - start).count();
    auto total_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Scheduled " << num_tasks << " tasks in " << schedule_duration << " microseconds"
              << std::endl;
    std::cout << "Total test duration: " << total_duration << " milliseconds" << std::endl;

    // 调度应该很快完成
    EXPECT_LT(schedule_duration, 10000); // 小于10ms
}