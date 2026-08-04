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

    // 重新调度周期性定时器（持锁版，供外部调用）
    void RescheduleRecurring(std::shared_ptr<TimerEventInterface> timer, Tick delay_ticks);

    // 重新调度周期性定时器（无锁版，仅在已持有 mutex_ 时调用，即 execute() 内）
    void RescheduleRecurringNoLock(TimerEventInterface *timer, Tick delay_ticks);

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

    /* 仅 timer 线程访问，无需加锁：execute() 收集待投递任务，
     * TimerLoop 在释放 mutex_ 后批量 enqueue，避免持锁期间触发 syscall */
    std::vector<std::function<void()>> pendingTasks_;

    /* true: 回调直接在 timer 线程执行（适合 pub，callback 快且无用户代码）
     * false: 投递到线程池（适合 sub，用户回调可能阻塞）*/
    bool direct_execute_{false};

    TimerScheduler(const TimerScheduler &) = delete;
    TimerScheduler &operator=(const TimerScheduler &) = delete;
};

// ============ 实现部分（通常在.cc文件中） ============

inline TimerScheduler::TimerScheduler(Tick tick_ms, size_t thread_pool_size)
    : mutex_(), active_timers_(), timer_wheel_(), tick_ms_(tick_ms),
      should_stop_(false), is_paused_(false), timer_thread_(),
      thread_pool_(thread_pool_size == 0 ? 1 : thread_pool_size),
      direct_execute_(thread_pool_size == 0)
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
    thread_pool_.enqueue_void([callback = std::move(callback)]() {
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

inline void TimerScheduler::RescheduleRecurringNoLock(TimerEventInterface *timer, Tick delay_ticks)
{
    // 调用方已持有 mutex_（execute() 在 advance() 内被调用）
    timer_wheel_.schedule(timer, delay_ticks);
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
            if (!is_paused_.load(std::memory_order_acquire)) {
                /*查询最近的到期 timer，一次 advance 跳过去，避免每 tick 空转*/
                Tick ticksToAdvance = 1;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ticksToAdvance = timer_wheel_.ticks_to_next_event(1000);
                    if (ticksToAdvance == 0)
                        ticksToAdvance = 1;
                    timer_wheel_.advance(ticksToAdvance);
                }
                // 锁已释放：direct_execute_=true 直接在 timer 线程跑（pub 场景，无线程池开销）
                // direct_execute_=false 投递到线程池（sub 场景，用户回调可能慢）
                if (direct_execute_) {
                    for (auto &task : pendingTasks_) {
                        try {
                            task();
                        } catch (const std::exception &e) {
                            std::cerr << "Timer callback error: " << e.what() << std::endl;
                        }
                    }
                } else if (!pendingTasks_.empty()) {
                    /*批量投递：一次 lock + 一次 notify，替代 N 次 enqueue_void*/
                    auto batch = std::move(pendingTasks_);
                    thread_pool_.enqueue_void([batch = std::move(batch)]() mutable {
                        for (auto &task : batch) {
                            try {
                                task();
                            } catch (const std::exception &e) {
                                std::cerr << "Timer callback error: " << e.what() << std::endl;
                            }
                        }
                    });
                }
                pendingTasks_.clear();

                /*跳过的 tick，对齐到下一个 tick 边界*/
                next_tick += kTickDuration * ticksToAdvance;
                if (now > next_tick) {
                    next_tick = now + kTickDuration;
                }
            } else {
                next_tick += kTickDuration;
                if (now > next_tick) {
                    next_tick = now + kTickDuration;
                }
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
    // 在 mutex_ 持有期间只做 push，不触发任何 syscall
    auto self = shared_from_this();
    scheduler_.pendingTasks_.push_back(
        [callback = callback_, &is_canceled = is_canceled_, self, &scheduler = scheduler_]() {
            if (!is_canceled.load(std::memory_order_acquire)) {
                callback();
            }
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
    // 重调度在 timer 线程内完成（mutex_ 已持有），不再从线程池线程抢锁
    scheduler_.RescheduleRecurringNoLock(this, interval_ticks_);

    // 只往 pendingTasks_ push，不触发 syscall
    auto self = shared_from_this();
    scheduler_.pendingTasks_.push_back(
        [self, callback = callback_, &is_canceled = is_canceled_]() {
            if (!is_canceled.load(std::memory_order_acquire)) {
                callback();
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
