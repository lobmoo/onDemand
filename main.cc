#include "BS_thread_pool.hpp" // BS::synced_stream, BS::thread_pool
#include <chrono>             // std::chrono
#include <thread>             // std::this_thread

BS::synced_stream sync_out;
BS::thread_pool pool(16);


void monitor_tasks()
{
    sync_out.println(pool.get_tasks_total(), " tasks total, ", pool.get_tasks_running(), " tasks running, ", pool.get_tasks_queued(), " tasks queued.");
}

int main()
{
    for (int i = 0; i < 100; ++i)
    {
        pool.detach_task([i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sync_out.println("Task ", i, " done.");
        });
        monitor_tasks();
    }
}