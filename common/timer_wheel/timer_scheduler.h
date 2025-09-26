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

#include "thread_pool.h"
#include "timer_wheel.h"

using Tick = uint64_t;

class TimerScheduler
{
public:
    using Callback = std::function<void()>;

    explicit TimerScheduler(Tick tick_ms = 10,
                            size_t thread_pool_size = std::thread::hardware_concurrency());

    ~TimerScheduler();

    // 调度单次执行的定时器
    std::shared_ptr<TimerEventInterface> Schedule(Callback callback, Tick delay_ticks);

    // 调度周期性执行的定时器
    std::shared_ptr<TimerEventInterface> ScheduleRecurring(Callback callback, Tick delay_ticks,
                                                           Tick interval_ticks);

    // 取消定时器
    void Cancel(std::shared_ptr<TimerEventInterface> timer);

    // 立即执行回调（异步）
    void Post(Callback callback);

private:
    // 单次执行定时器实现
    class CallbackTimer : public TimerEventInterface
    {
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

    // 重新调度周期性定时器（内部使用）
    void RescheduleRecurring(std::shared_ptr<TimerEventInterface> timer, Tick delay_ticks);

    // 定时器主循环
    void TimerLoop();

    ThreadPool thread_pool_;
    TimerWheel timer_wheel_;
    std::atomic<bool> should_stop_;
    std::thread timer_thread_;
    std::mutex mutex_;
    Tick tick_ms_; // 时钟周期（毫秒）
    std::vector<std::shared_ptr<TimerEventInterface>> active_timers_;

    // 禁用拷贝构造和拷贝赋值
    TimerScheduler(const TimerScheduler &) = delete;
    TimerScheduler &operator=(const TimerScheduler &) = delete;
};

// ============ 实现部分（通常在.cc文件中） ============

inline TimerScheduler::TimerScheduler(Tick tick_ms, size_t thread_pool_size)
    : thread_pool_(thread_pool_size), timer_wheel_(), should_stop_(false), tick_ms_(tick_ms)
{
    timer_thread_ = std::thread([this]() { TimerLoop(); });
}

inline TimerScheduler::~TimerScheduler()
{
    should_stop_.store(true, std::memory_order_release);
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

inline std::shared_ptr<TimerEventInterface> TimerScheduler::Schedule(Callback callback,
                                                                     Tick delay_ticks)
{
    auto timer = std::make_shared<CallbackTimer>(std::move(callback), *this);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            timer_wheel_.schedule(timer.get(), delay_ticks);

            // 检查是否已经在活跃列表中，避免重复添加
            if (std::find(active_timers_.begin(), active_timers_.end(), timer)
                == active_timers_.end()) {
                active_timers_.push_back(timer);
            }
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

            if (std::find(active_timers_.begin(), active_timers_.end(), timer)
                == active_timers_.end()) {
                active_timers_.push_back(timer);
            }
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
        active_timers_.erase(std::remove(active_timers_.begin(), active_timers_.end(), timer),
                             active_timers_.end());
    }
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

    if (std::find(active_timers_.begin(), active_timers_.end(), timer) == active_timers_.end()) {
        active_timers_.push_back(timer);
    }
}

inline void TimerScheduler::TimerLoop()
{
    using Clock = std::chrono::steady_clock;
    auto next_tick = Clock::now();
    const auto kTickDuration = std::chrono::milliseconds(static_cast<int64_t>(tick_ms_));

    while (!should_stop_.load(std::memory_order_acquire)) {
        auto now = Clock::now();

        if (now >= next_tick) {
            {
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

    scheduler_.Post([callback = callback_, &is_canceled = is_canceled_]() {
        if (!is_canceled.load(std::memory_order_acquire)) {
            callback();
        }
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

    // 执行回调
    scheduler_.Post([callback = callback_, &is_canceled = is_canceled_]() {
        if (!is_canceled.load(std::memory_order_acquire)) {
            callback();
        }
    });

    // 重新调度下一次执行
    if (!is_canceled_.load(std::memory_order_acquire)) {
        scheduler_.Post([self = shared_from_this()]() {
            self->scheduler_.RescheduleRecurring(self, self->interval_ticks_);
        });
    }
}

inline void TimerScheduler::RecurringCallbackTimer::cancel()
{
    is_canceled_.store(true, std::memory_order_release);
    TimerEventInterface::cancel();
}

// ============ 使用示例 ============

/*
#include "log/logger.h"

int main() {
    Logger::GetInstance()->Init("log/myapp.log", Logger::kConsole, Logger::kInfo, 60, 5);

    TimerScheduler scheduler(10, 4);  // 10ms tick, 4 threads
    int count = 0;

    // 测试单次任务
    auto timer1 = scheduler.Schedule(
        [&count]() { 
            LOG(Info) << "Single task executed, count: " << ++count; 
        }, 
        100);  // 100 ticks = 1秒后执行

    // 测试周期性任务
    auto timer2 = scheduler.ScheduleRecurring(
        [&count]() { 
            LOG(Info) << "Recurring task executed, count: " << ++count; 
        },
        200,   // 200 ticks = 2秒后开始
        400);  // 每400 ticks = 每4秒执行一次

    // 运行一段时间
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 取消周期性任务
    scheduler.Cancel(timer2);
    LOG(Info) << "Cancelled recurring timer";

    // 继续运行观察效果
    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}
*/