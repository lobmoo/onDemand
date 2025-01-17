#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <queue>
#include <thread>

class MessageProducer {
public:
    boost::signals2::signal<void(const std::string&)> newMessage;

    void produceMessage(const std::string& msg) {
        std::cout << "Producing message: " << msg << std::endl;
        newMessage(msg);  // 触发信号
    }
};

class MessageConsumer {
public:
    void consumeMessage(const std::string& msg) {
        std::cout << "Consuming message: " << msg << std::endl;
    }

    void consumeMessageAsync(const std::string& msg) {
        // 让消息消费过程异步执行
        std::thread([this, msg] { consumeMessage(msg); }).detach();  // 异步执行槽函数
    }
};

int main() {
    MessageProducer producer;
    MessageConsumer consumer;

    // 将消费者的异步槽函数连接到信号
    producer.newMessage.connect(boost::bind(&MessageConsumer::consumeMessageAsync, &consumer, _1));

    // 模拟生产者产生消息
    producer.produceMessage("Hello, Boost Signals!");
    std::this_thread::sleep_for(std::chrono::seconds(1));

    producer.produceMessage("Another message");
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
