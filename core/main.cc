#include <iostream>
#include <thread>
#include "fifo.h"
#include "log/logger.h"

int main()
{
    Logger::Instance()->Init(" ");
    try {
        fifo::Fifo::Params params{512 * 1024};
        fifo::Fifo fifo(params);
        auto user = fifo.add_user();

        std::thread reader([&] {
            try {
                char buffer[128];
                size_t len =
                    fifo.read(user.get(), nullptr, nullptr, buffer, sizeof(buffer), false, -1);
                buffer[std::min(len, sizeof(buffer) - 1)] = '\0';
                std::cout << "Read: " << buffer << "\n";
            } catch (const fifo::FifoException &e) {
                std::cerr << "Error: " << e.what() << "\n";
            }
        });
        while (1) {
            fifo.write(0, 0, "test_string", 12);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        // reader.join();
    } catch (const fifo::FifoException &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
