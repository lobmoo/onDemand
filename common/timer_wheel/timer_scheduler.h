/**
 * @file timer_scheduler.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-09-29
 * 
 * @copyright Copyright (c) 2025  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-09-29     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_set>

#include "thread_pool.h"
#include "timer_wheel.h"

/*
TimerScheduler 架构:
┌─────────────────┐
│   User Code     │ 
├─────────────────┤
│ TimerScheduler  │  ← 高级接口封装
├─────────────────┤  
│   TimerWheel    │  ← 底层时间轮算法
├─────────────────┤
│   ThreadPool    │  ← 异步执行引擎
└─────────────────┘
*/

using Tick = uint64_t;

class TimerScheduler
{
public:
    using Callback = std::function<void()>;

    explicit TimerScheduler(Tick tick_ms = 10,
                            size_t thread_pool_size = std::thread::hardware_concurrency() / 4);

    ~TimerScheduler();

    // 调度单次执行的定时器
    std::shared_ptr<TimerEventInterface> Schedule(Callback callback, Tick delay_ticks);

    // 调度周期性执行的定时器
    std::shared_ptr<TimerEventInterface> ScheduleRecurring(Callback callback, Tick delay_ticks,
                                                           Tick interval_ticks);

    // 取消定时器
    void Cancel(std::shared_ptr<TimerEventInterface> timer);

    // 暂停调度器（所有定时器停止触发，但不清除）
    void Pause();

    // 恢复调度器
    void Resume();

    // 停止调度器（等同于析构，停止所有定时器）
    void Stop();

    // 立即执行回调（异步）
    void Post(Callback callback);

private:
    // 单次执行定时器实现
    class CallbackTimer : public TimerEventInterface,
                           public std::enable_shared_from_this<CallbackTimer>
    {
        friend class TimerScheduler;

    public:
        CallbackTimer(Callback callback, TimerScheduler &scheduler);
        ~CallbackTimer() override = default;

        void execute() override;
        void cancel();

    private:
        Callback callback_;
        TimerScheduler &scheduler_;
        std::atomic<bool> is_canceled_;
        
    };

    // 周期性执行定时器实现
    class RecurringCallbackTimer : public TimerEventInterface,
                                   public std::enable_shared_from_this<RecurringCallbackTimer>
    {
    public:
        RecurringCallbackTimer(Callback callback, TimerScheduler &scheduler, Tick interval_ticks);
        ~RecurringCallbackTimer() override = default;

        void execute() override;
        void cancel();

    private:
        Callback callback_;
        TimerScheduler &scheduler_;
        Tick interval_ticks_;
        std::atomic<bool> is_canceled_;
    };

    // 从活跃列表中移除（内部使用）
    void RemoveFromActive(std::shared_ptr<TimerEventInterface> timer);

    // 重新调度周期性定时器（内部使用）
    void RescheduleRecurring(std::shared_ptr<TimerEventInterface> timer, Tick delay_ticks);

    // 定时器主循环
    void TimerLoop();

    ThreadPool thread_pool_;
    TimerWheel timer_wheel_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> is_paused_;  // 暂停标志
    std::thread timer_thread_;
    std::mutex mutex_;
    Tick tick_ms_; // 时钟周期（毫秒）
    std::unordered_set<std::shared_ptr<TimerEventInterface>> active_timers_;

   
    TimerScheduler(const TimerScheduler &) = delete;
    TimerScheduler &operator=(const TimerScheduler &) = delete;
};

// ============ 实现部分（通常在.cc文件中） ============

inline TimerScheduler::TimerScheduler(Tick tick_ms, size_t thread_pool_size)
    : mutex_(), active_timers_(), timer_wheel_(), tick_ms_(tick_ms), 
      should_stop_(false), is_paused_(false), timer_thread_(), thread_pool_(thread_pool_size)
{
    timer_thread_ = std::thread([this]() { TimerLoop(); });
}

inline TimerScheduler::~TimerScheduler()
{
    // 先停止定时器线程
    should_stop_.store(true, std::memory_order_release);
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
    
    // 取消所有活跃的周期性定时器，防止悬空引用
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& timer : active_timers_) {
        if (timer) {
            timer->cancel();
        }
    }
    active_timers_.clear();
}

inline std::shared_ptr<TimerEventInterface> TimerScheduler::Schedule(Callback callback,
                                                                     Tick delay_ticks)
{
    auto timer = std::make_shared<CallbackTimer>(std::move(callback), *this);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            timer_wheel_.schedule(timer.get(), delay_ticks);
            active_timers_.insert(timer);
        } catch (const std::exception &e) {
            std::cerr << "Error scheduling timer: " << e.what() << std::endl;
            throw;
        }
    }
    return timer;
}

inline std::shared_ptr<TimerEventInterface>
TimerScheduler::ScheduleRecurring(Callback callback, Tick delay_ticks, Tick interval_ticks)
{
    auto timer =
        std::make_shared<RecurringCallbackTimer>(std::move(callback), *this, interval_ticks);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            timer_wheel_.schedule(timer.get(), delay_ticks);
            active_timers_.insert(timer);  
        } catch (const std::exception &e) {
            std::cerr << "Error scheduling recurring timer: " << e.what() << std::endl;
            throw;
        }
    }

    return timer;
}

inline void TimerScheduler::Cancel(std::shared_ptr<TimerEventInterface> timer)
{
    if (!timer) {
        return;
    }

    timer->cancel();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_timers_.erase(timer);
    }
}

inline void TimerScheduler::Pause()
{
    is_paused_.store(true, std::memory_order_release);
}

inline void TimerScheduler::Resume()
{
    is_paused_.store(false, std::memory_order_release);
}

inline void TimerScheduler::Stop()
{
    should_stop_.store(true, std::memory_order_release);
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& timer : active_timers_) {
        if (timer) {
            timer->cancel();
        }
    }
    active_timers_.clear();
}

inline void TimerScheduler::Post(Callback callback)
{
    thread_pool_.enqueue([callback = std::move(callback)]() {
        try {
            callback();
        } catch (const std::exception &e) {
            std::cerr << "Error executing posted callback: " << e.what() << std::endl;
        }
    });
}

inline void TimerScheduler::RescheduleRecurring(std::shared_ptr<TimerEventInterface> timer,
                                                Tick delay_ticks)
{
    std::lock_guard<std::mutex> lock(mutex_);
    timer_wheel_.schedule(timer.get(), delay_ticks);
}

inline void TimerScheduler::TimerLoop()
{
    pthread_setname_np(pthread_self(), "TimerScheduler");
    using Clock = std::chrono::steady_clock;
    auto next_tick = Clock::now();
    const auto kTickDuration = std::chrono::milliseconds(static_cast<int64_t>(tick_ms_));

    while (!should_stop_.load(std::memory_order_acquire)) {
        auto now = Clock::now();

        if (now >= next_tick) {
            // 如果处于暂停状态，跳过定时器处理
            if (!is_paused_.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> lock(mutex_);

                // 限制每次tick最多处理的定时器数量，避免阻塞过久
                constexpr size_t kMaxExecutePerTick = 10;
                timer_wheel_.advance(1, kMaxExecutePerTick);
            }

            next_tick += kTickDuration;

            // 如果系统延迟导致错过了多个tick，重新对齐到下一个tick
            if (now > next_tick) {
                next_tick = now + kTickDuration;
            }
        }

        std::this_thread::sleep_until(next_tick);
    }
}

// ============ CallbackTimer 实现 ============

inline TimerScheduler::CallbackTimer::CallbackTimer(Callback callback, TimerScheduler &scheduler)
    : callback_(std::move(callback)), scheduler_(scheduler), is_canceled_(false)
{
}

inline void TimerScheduler::CallbackTimer::execute()
{
    if (is_canceled_.load(std::memory_order_acquire)) {
        return;
    }

    auto self = shared_from_this();
    scheduler_.Post([callback = callback_, &is_canceled = is_canceled_, self, &scheduler = scheduler_]() {
        if (!is_canceled.load(std::memory_order_acquire)) {
            callback();
        }
        // 执行完后从活跃列表移除
        scheduler.RemoveFromActive(self);
    });
}

inline void TimerScheduler::CallbackTimer::cancel()
{
    is_canceled_.store(true, std::memory_order_release);
    TimerEventInterface::cancel();
}

// ============ RecurringCallbackTimer 实现 ============

inline TimerScheduler::RecurringCallbackTimer::RecurringCallbackTimer(Callback callback,
                                                                      TimerScheduler &scheduler,
                                                                      Tick interval_ticks)
    : callback_(std::move(callback)), scheduler_(scheduler), interval_ticks_(interval_ticks),
      is_canceled_(false)
{
}

inline void TimerScheduler::RecurringCallbackTimer::execute()
{
    if (is_canceled_.load(std::memory_order_acquire)) {
        return;
    }

    auto self = shared_from_this();
    
    // 执行回调并重新调度（原子操作，避免中间被取消导致不一致）
    scheduler_.Post([callback = callback_, &is_canceled = is_canceled_, self]() {
        if (!is_canceled.load(std::memory_order_acquire)) {
            callback();
        }
        // 执行完后重新调度下一次（无论回调是否执行）
        if (!is_canceled.load(std::memory_order_acquire)) {
            self->scheduler_.RescheduleRecurring(self, self->interval_ticks_);
        }
    });
}

inline void TimerScheduler::RecurringCallbackTimer::cancel()
{
    is_canceled_.store(true, std::memory_order_release);
    TimerEventInterface::cancel();
}


inline void TimerScheduler::RemoveFromActive(std::shared_ptr<TimerEventInterface> timer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    active_timers_.erase(timer);
}


// ============ 使用示例 TEST ============

#if 0
class TimerTester {
private:
    std::atomic<int> counter_{0};
    std::atomic<int> single_executed_{0};
    std::atomic<int> recurring_executed_{0};
    std::atomic<int> post_executed_{0};
    
public:
    void TestBasicFunctionality() {
        std::cout << "=== 测试基本功能 ===" << std::endl;
        
        TimerScheduler scheduler(10, 2); // 10ms tick, 2个工作线程
        
        std::cout << "创建TimerScheduler成功 (10ms tick, 2 threads)" << std::endl;
        
        // 测试立即执行
        scheduler.Post([this]() {
            post_executed_++;
            std::cout << "Post任务执行完成" << std::endl;
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (post_executed_ == 1) {
            std::cout << "✅ Post功能测试通过" << std::endl;
        } else {
            std::cout << "❌ Post功能测试失败" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void TestSingleTimer() {
        std::cout << "=== 测试单次定时器 ===" << std::endl;
        
        TimerScheduler scheduler(10, 2);
        single_executed_ = 0;
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 调度一个1秒后执行的任务 (100 ticks * 10ms = 1000ms)
        auto timer = scheduler.Schedule([this, start_time]() {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            single_executed_++;
            std::cout << "单次定时器执行，延迟: " << duration.count() << "ms" << std::endl;
        }, 100);
        
        std::cout << "调度单次定时器 (1000ms后执行)" << std::endl;
        
        // 等待1.2秒确保任务执行
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        
        if (single_executed_ == 1) {
            std::cout << "✅ 单次定时器测试通过" << std::endl;
        } else {
            std::cout << "❌ 单次定时器测试失败, 执行次数: " << single_executed_ << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void TestRecurringTimer() {
        std::cout << "=== 测试周期性定时器 ===" << std::endl;
        
        TimerScheduler scheduler(10, 2);
        recurring_executed_ = 0;
        
        // 调度一个500ms后开始，每500ms执行一次的任务
        auto timer = scheduler.ScheduleRecurring([this]() {
            recurring_executed_++;
            std::cout << "周期性定时器执行，第 " << recurring_executed_ << " 次" << std::endl;
        }, 50, 50); // 500ms延迟，每500ms执行一次
        
        std::cout << "调度周期性定时器 (500ms后开始，每500ms执行一次)" << std::endl;
        
        // 运行2.5秒，应该执行3-4次
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        
        int executions = recurring_executed_;
        
        // 取消定时器
        scheduler.Cancel(timer);
        std::cout << "取消周期性定时器" << std::endl;
        
        // 再等待1秒，确认不会继续执行
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        if (executions >= 3 && recurring_executed_ == executions) {
            std::cout << "✅ 周期性定时器测试通过 (执行 " << executions << " 次)" << std::endl;
        } else {
            std::cout << "❌ 周期性定时器测试失败" << std::endl;
            std::cout << "   取消前执行: " << executions << " 次" << std::endl;
            std::cout << "   取消后执行: " << recurring_executed_ << " 次" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void TestTimerCancel() {
        std::cout << "=== 测试定时器取消 ===" << std::endl;
        
        TimerScheduler scheduler(10, 2);
        std::atomic<int> canceled_timer_executed{0};
        
        // 调度一个2秒后执行的任务
        auto timer = scheduler.Schedule([&canceled_timer_executed]() {
            canceled_timer_executed++;
            std::cout << "这个任务不应该执行!" << std::endl;
        }, 200); // 2秒后执行
        
        std::cout << "调度单次定时器 (2000ms后执行)" << std::endl;
        
        // 等待500ms后取消
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        scheduler.Cancel(timer);
        std::cout << "取消定时器 (500ms后)" << std::endl;
        
        // 再等待2秒确认任务不会执行
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        if (canceled_timer_executed == 0) {
            std::cout << "✅ 定时器取消测试通过" << std::endl;
        } else {
            std::cout << "❌ 定时器取消测试失败，已取消的定时器仍然执行了" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void TestMultipleTimers() {
        std::cout << "=== 测试多个定时器 ===" << std::endl;
        
        TimerScheduler scheduler(10, 4); // 增加线程数
        std::vector<std::atomic<int>> counters(5);
        std::vector<std::shared_ptr<TimerEventInterface>> timers;
        
        // 创建5个不同延迟的定时器
        for (int i = 0; i < 5; ++i) {
            timers.push_back(scheduler.Schedule([&counters, i]() {
                counters[i]++;
                std::cout << "定时器 " << i << " 执行 (延迟 " << (i+1)*200 << "ms)" << std::endl;
            }, (i+1) * 20)); // 200ms, 400ms, 600ms, 800ms, 1000ms
        }
        
        std::cout << "创建5个定时器，延迟分别为 200ms, 400ms, 600ms, 800ms, 1000ms" << std::endl;
        
        // 等待所有任务完成
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        
        bool all_executed = true;
        for (int i = 0; i < 5; ++i) {
            if (counters[i] != 1) {
                all_executed = false;
                break;
            }
        }
        
        if (all_executed) {
            std::cout << "✅ 多个定时器测试通过" << std::endl;
        } else {
            std::cout << "❌ 多个定时器测试失败" << std::endl;
            for (int i = 0; i < 5; ++i) {
                std::cout << "   定时器 " << i << " 执行次数: " << counters[i] << std::endl;
            }
        }
        
        std::cout << std::endl;
    }
    
    void TestHighFrequencyTimers() {
        std::cout << "=== 测试高频定时器 ===" << std::endl;
        
        TimerScheduler scheduler(1, 4); // 1ms tick, 更高精度
        std::atomic<int> high_freq_counter{0};
        
        // 创建一个每50ms执行一次的高频定时器
        auto timer = scheduler.ScheduleRecurring([&high_freq_counter]() {
            high_freq_counter++;
            if (high_freq_counter % 10 == 0) {
                std::cout << "高频定时器执行 " << high_freq_counter << " 次" << std::endl;
            }
        }, 10, 5); // 10ms后开始，每5ms执行一次
        
        std::cout << "创建高频定时器 (每5ms执行一次)" << std::endl;
        
        // 运行500ms
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        scheduler.Cancel(timer);
        
        // 理论上应该执行约100次 (500ms / 5ms)
        int executions = high_freq_counter;
        std::cout << "高频定时器总执行次数: " << executions << std::endl;
        
        if (executions >= 80 && executions <= 120) { // 允许一定误差
            std::cout << "✅ 高频定时器测试通过" << std::endl;
        } else {
            std::cout << "❌ 高频定时器测试失败，执行次数超出预期范围" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void TestStressLoad() {
        std::cout << "=== 测试压力负载 ===" << std::endl;
        
        TimerScheduler scheduler(5, 8); // 5ms tick, 8个线程
        std::atomic<int> stress_counter{0};
        std::vector<std::shared_ptr<TimerEventInterface>> stress_timers;
        
        std::cout << "创建100个随机延迟的定时器..." << std::endl;
        
        // 创建100个随机延迟的定时器
        for (int i = 0; i < 100; ++i) {
            int delay = (i % 50) + 1; // 1-50的随机延迟
            stress_timers.push_back(scheduler.Schedule([&stress_counter, i]() {
                stress_counter++;
                if (stress_counter % 20 == 0) {
                    std::cout << "压力测试: 已完成 " << stress_counter << " 个任务" << std::endl;
                }
            }, delay));
        }
        
        // 等待所有任务完成
        std::this_thread::sleep_for(std::chrono::milliseconds(300)); // 50*5ms = 250ms + 缓冲
        
        if (stress_counter == 100) {
            std::cout << "✅ 压力负载测试通过 (100/100 任务完成)" << std::endl;
        } else {
            std::cout << "❌ 压力负载测试失败 (" << stress_counter << "/100 任务完成)" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void RunAllTests() {
        std::cout << "开始TimerScheduler功能测试...\n" << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        TestBasicFunctionality();
        TestSingleTimer();
        TestRecurringTimer();
        TestTimerCancel();
        TestMultipleTimers();
        TestHighFrequencyTimers();
        TestStressLoad();
        
        auto end_time = std::chrono::steady_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "=== 测试总结 ===" << std::endl;
        std::cout << "总测试时间: " << total_time.count() << "ms" << std::endl;
        std::cout << "TimerScheduler功能测试完成!" << std::endl;
    }
};


int main() {
    try {
        TimerTester tester;
        tester.RunAllTests();
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
#endif
