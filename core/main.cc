#include <iostream>
#include <string>
#include <vector>
#include "fifo.h"
#include "log/logger.h"
#include "timer_wheel/thread_pool.h"
#include "timer_wheel/timer_scheduler.h"

int main()
{
    bool result = Logger::GetInstance()->Init("");

    ThreadPool pool(4);
    pool.enqueue([result]() {
        while(1)
        {
            LOG(info) << "Thread pool task is running";
             LOG(trace) << "Thread pool task is running";
              LOG(debug) << "Thread pool task is running";
               LOG(error) << "Thread pool task is running";
                LOG(critical) << "Thread pool task is running";
                 LOG(warning) << "Thread pool task is running";
                    LOG(critical) << "Thread pool task is running  : " << result;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
