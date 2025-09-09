#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>
#include <iostream>
#include "BS_thread_pool.hpp"
#include "timer_wheel.h"

using Tick = uint64_t;

class TimerScheduler
{
public:
    using Callback = std::function<void()>;

    explicit TimerScheduler(Tick tick = 10,
                            size_t thread_pool_size = std::thread::hardware_concurrency())
        : thread_pool_(thread_pool_size), tick_(tick), timers_(), stop_flag_(false)
    {
        timer_thread_ = std::thread([this]() { this->timerLoop(); });
    }

    ~TimerScheduler()
    {
        stop_flag_.store(true);
        if (timer_thread_.joinable())
            timer_thread_.join();
        thread_pool_.wait();
    }

    std::shared_ptr<TimerEventInterface> schedule(Callback cb, Tick delay_ticks)
    {
        auto timer = std::make_shared<CallbackTimer>(std::move(cb), *this);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            try {
                timers_.schedule(timer.get(), delay_ticks);
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

    std::shared_ptr<TimerEventInterface> scheduleRecurring(Callback cb, Tick delay_ticks,
                                                           Tick interval_ticks)
    {
        auto timer = std::make_shared<RecurringCallbackTimer>(std::move(cb), *this, interval_ticks);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            try {
                timers_.schedule(timer.get(), delay_ticks);
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

    void cancel(std::shared_ptr<TimerEventInterface> timer)
    {
        if (timer) {
            timer->cancel();
            std::lock_guard<std::mutex> lock(mutex_);
            active_timers_.erase(std::remove(active_timers_.begin(), active_timers_.end(), timer),
                                 active_timers_.end());
        }
    }

    void post(Callback cb)
    {
        thread_pool_.detach_task([cb = std::move(cb)]() {
            try {
                cb();
            } catch (const std::exception &e) {
                std::cerr << "Error executing callback: " << e.what() << std::endl;
            }
        });
    }

    void rescheduleRecurring(std::shared_ptr<TimerEventInterface> timer, Tick delay_ticks)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        timers_.schedule(timer.get(), delay_ticks);
        if (std::find(active_timers_.begin(), active_timers_.end(), timer)
            == active_timers_.end()) {
            active_timers_.push_back(timer);
        }
    }

private:
    class RecurringCallbackTimer : public TimerEventInterface,
                                   public std::enable_shared_from_this<RecurringCallbackTimer>
    {
    public:
        RecurringCallbackTimer(Callback cb, TimerScheduler &sched, Tick interval)
            : callback_(std::move(cb)), scheduler_(sched), interval_ticks_(interval),
              canceled_(false)
        {
        }

        void execute() override
        {
            if (canceled_.load(std::memory_order_acquire)) {
                return;
            }
            scheduler_.post([cb = callback_, &canceled = canceled_]() {
                if (!canceled.load(std::memory_order_acquire)) {
                    cb();
                }
            });
            if (!canceled_.load(std::memory_order_acquire)) {
                scheduler_.post([self = shared_from_this()]() {
                    self->scheduler_.rescheduleRecurring(self, self->interval_ticks_);
                });
            }
        }

        void cancel()
        {
            canceled_.store(true, std::memory_order_release);
            TimerEventInterface::cancel();
        }

    private:
        Callback callback_;
        TimerScheduler &scheduler_;
        Tick interval_ticks_;
        std::atomic<bool> canceled_;
    };

    class CallbackTimer : public TimerEventInterface
    {
    public:
        CallbackTimer(Callback cb, TimerScheduler &sched)
            : callback_(std::move(cb)), scheduler_(sched), canceled_(false)
        {
        }

        void execute() override
        {
            if (canceled_.load(std::memory_order_acquire)) {
                return;
            }
            scheduler_.post([cb = callback_, &canceled = canceled_]() {
                if (!canceled.load(std::memory_order_acquire)) {
                    cb();
                }
            });
        }

        void cancel()
        {
            canceled_.store(true, std::memory_order_release);
            TimerEventInterface::cancel();
        }

    private:
        Callback callback_;
        TimerScheduler &scheduler_;
        std::atomic<bool> canceled_;
    };

    void timerLoop()
    {
        using clock = std::chrono::steady_clock;
        auto next_tick = clock::now();
        const auto tick_duration = std::chrono::milliseconds(static_cast<int64_t>(tick_));

        while (!stop_flag_.load()) {
            auto now = clock::now();
            if (now >= next_tick) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    size_t max_execute = thread_pool_.get_thread_count() * 10;
                    timers_.advance(1, max_execute);
                }
                next_tick += tick_duration;
                if (now > next_tick) {
                    next_tick = now + tick_duration;
                }
            }
            std::this_thread::sleep_until(next_tick);
        }
    }

private:
    BS::thread_pool<> thread_pool_;
    TimerWheel timers_;
    std::atomic<bool> stop_flag_;
    std::thread timer_thread_;
    std::mutex mutex_;
    Tick tick_; //ms
    std::vector<std::shared_ptr<TimerEventInterface>> active_timers_;
};



//Example：

// int main()
// {
//     Logger::Instance()->Init("log/myapp.log", Logger::console, Logger::info, 60, 5);

//     TimerScheduler scheduler(10, 4); // 10ms tick, 4 threads
//     int count = 0;

//     // 测试单次任务
//     auto timer1 = scheduler.schedule(
//         [&count]() { LOG(info) << "Single task executed, count: " << ++count; }, 100);

//     // 测试周期性任务
//     auto timer2 = scheduler.scheduleRecurring(
//         [&count]() { LOG(info) << "Recurring task executed, count: " << ++count; },
//         200, 400);

//     // 等待一段时间后取消
//     // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//     // scheduler.cancel(timer2);

//     // 继续运行一段时间以观察取消效果
//     while (1) {
//         LOG(error) << "Running main loop, count: " << count;
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }

//     return 0;
// }
