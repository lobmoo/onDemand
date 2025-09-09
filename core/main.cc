#include <iostream>
#include <string>
#include <vector>
#include "fifo.h"
#include "log/logger.h"
#include "timer_wheel/timer_scheduler.h"

int main()
{
    Logger::Instance()->Init("log/myapp.log", Logger::console, Logger::info, 60, 5);

    TimerScheduler scheduler(10, 4); // 10ms tick, 4 threads
    int count = 0;

    // 测试单次任务
    auto timer1 = scheduler.schedule(
        [&count]() { LOG(info) << "Single task executed, count: " << ++count; }, 100);

    // 测试周期性任务
    auto timer2 = scheduler.scheduleRecurring(
        [&count]() { LOG(info) << "Recurring task executed, count: " << ++count; },
        200, 400);

    // 等待一段时间后取消
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // scheduler.cancel(timer2);

    // 继续运行一段时间以观察取消效果
    while (1) {
        LOG(error) << "Running main loop, count: " << count;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
