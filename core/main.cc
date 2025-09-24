#include <iostream>
#include <string>
#include <vector>
#include "fifo.h"
#include "log/logger.h"
#include "timer_wheel/thread_pool.h"
#include "timer_wheel/timer_scheduler.h"

int main()
{
    Logger::Instance()->Init("log/myapp.log", Logger::console, Logger::info, 60, 5);

    ThreadPool pool(4);
    pool.enqueue([]() {
        while(1)
        {
            LOG(info) << "Thread pool task is running";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    ///std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
