#include <unistd.h>

#include <iostream>
#include <thread>
#include <vector>

#include "log/logger.h"
#include "request_reply_api/DDSRequestReplyClient.h"
#include "request_reply_api/DDSRequestReplyServer.h"

#include "RequestReplyTypes/Calculator.hpp"
#include "RequestReplyTypes/CalculatorPubSubTypes.hpp"

#include "1k/Calculator_1k.hpp"
#include "1k/Calculator_1kPubSubTypes.hpp"


#include "100k/Calculator_100k.hpp"
#include "100k/Calculator_100kPubSubTypes.hpp"


#include "1M/Calculator_1M.hpp"
#include "1M/Calculator_1MPubSubTypes.hpp"




bool reply_callback(const CalculatorReplyType &reply, const SampleInfo &info)
{

    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    LOG(warning) << "+++++++++++++++++[dealy] Reply recrvied whith result: " << timestamp - reply.timestamp() << "ms";
    static int count = 0;
    count++;
    LOG(info) << "+++++++++++++++++++++++Custom callback: Reply received with result '"
              << reply.result() << "'";
    if (count > 100) {
        return true;
    }
    return false;
};

void request_callback(const CalculatorRequestType &request, CalculatorReplyType &reply)
{
    int result = 0;
    result = request.x() + request.y();
    reply.client_id("x");
    reply.result(result);
    LOG(info) << "+++++++++++++++++++++++[SVR]request_callback";
    // std::this_thread::sleep_for(std::chrono::seconds(10));
};

void request_callback2(const CalculatorRequestType &request, CalculatorReplyType &reply)
{
    int result = 0;
    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    LOG(warning) << "+++++++++++++++++[dealy] result recrvied whith reply: " << timestamp - request.timestamp() << "ms    "  << timestamp << "  ::  " << request.timestamp();
    result = request.x() + request.y();
    reply.client_id("x");
    reply.timestamp(timestamp);
    reply.result(result);
    LOG(info) << "----------------------[SVR]request_callback2";
    // std::this_thread::sleep_for(std::chrono::seconds(10));
};

int main(int argc, char *argv[])
{
    Logger::Instance().setFlushOnLevel(Logger::trace);
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " svr/cli" << std::endl;
        return -1;
    }
    if (strcmp(argv[1], "svr") == 0) {
        auto ptr = std::make_shared<request_reply::DDSRequestReplyServer<
            CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType,
            CalculatorReplyType>>("SERVER_NAME", request_callback2);
        while (std::cin.get() != '\n') {
        }
    } else if (strcmp(argv[1], "cli") == 0) {
        auto ptr4 = std::make_shared<request_reply::DDSRequestReplyClient<
            CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType,
            CalculatorReplyType>>("SERVER_NAME");
        CalculatorRequestType request;
        short index = 0;

        while(index < 100)
        {
            request.client_id();
            request.x(index);
            request.y(100);
            request.operation(CalculatorOperationType::ADDITION);
            auto now = std::chrono::system_clock::now();
            auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
            auto epoch = value.time_since_epoch();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
            request.timestamp(timestamp);
            CalculatorReplyType response;
            if (!ptr4->send_request(request, reply_callback)) {
                LOG(error) << "send request failed";
            }
            index ++;
        }
       

        while (std::cin.get() != '\n') {
        }
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }
    return 0;
}



