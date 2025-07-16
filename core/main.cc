#include <iostream>
#include <string>
#include <vector>
#include "fifo.h"

int main()
{
    try {
        // 创建FIFO队列
        fifo::Fifo::Params params{1024};
        fifo::Fifo fifo(params);
        std::cout << "FIFO created with size=" << params.size << "\n";

        // 添加用户
        auto user1 = fifo.add_user(true); // 从头开始读取
        std::cout << "User1 created successfully\n";

        // 克隆用户
        auto user2 = fifo.clone_user(user1.get());
        std::cout << "User2 cloned successfully\n";

        // 写入数据
        std::string data = "Hello, FIFO!";
        fifo.write(1, 0, data.data(), data.size());
        std::cout << "Data written: " << data << "\n";

        // 读取数据（user1）
        int32_t cmd, arg1;
        std::vector<char> buffer(128);
        size_t len = fifo.read(user1.get(), &cmd, &arg1, buffer.data(), buffer.size(), false, 1000);
        std::string received(buffer.data(), len);
        std::cout << "User1 read: cmd=" << cmd << ", arg1=" << arg1 << ", data=" << received
                  << "\n";

        // 读取数据（user2）
        len = fifo.read(user2.get(), &cmd, &arg1, buffer.data(), buffer.size(), false, 1000);
        received = std::string(buffer.data(), len);
        std::cout << "User2 read: cmd=" << cmd << ", arg1=" << arg1 << ", data=" << received
                  << "\n";

        // 移除用户
        fifo.remove_user(user1.get());
        std::cout << "User1 removed successfully\n";
        fifo.remove_user(user2.get());
        std::cout << "User2 removed successfully\n";
    } catch (const fifo::FifoException &e) {
        std::cerr << "Error: " << e.what() << " (Code: " << static_cast<int>(e.code()) << ")\n";
    }
    return 0;
}