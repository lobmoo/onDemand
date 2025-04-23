// #include <unistd.h>

// #include <iostream>
// #include <thread>
// #include <vector>

// #include "log/logger.h"
// #include "request_reply_api/DDSRequestReplyClient.h"
// #include "request_reply_api/DDSRequestReplyServer.h"

// #include "RequestReplyTypes/Calculator.hpp"
// #include "RequestReplyTypes/CalculatorPubSubTypes.hpp"

// #include "config/xmlConfig.h" 

// void reply_callback(const CalculatorReplyType& reply, const SampleInfo& info) {
//   LOG(info) << "+++++++++++++++++++++++Custom callback: Reply received with result '" << reply.result() << "'";
// };


// void request_callback(const CalculatorRequestType& request, CalculatorReplyType& reply) {
//   int result = 0;
//   result = request.x() + request.y();
//   reply.client_id("x");
//   reply.result(result);
//   LOG(info) << "+++++++++++++++++++++++request_callback";
// };


// int main(int argc, char* argv[]) {
//   Logger::Instance().setFlushOnLevel(Logger::trace);
//   Logger::Instance().Init("log/myapp.log", Logger::both, Logger::trace, 60, 5);

//   if (argc < 2) {
//     std::cerr << "Usage: " << argv[0] << " svr/cli" << std::endl;
//     return -1;
//   }
//   if (strcmp(argv[1], "svr") == 0) {
//     auto ptr = std::make_shared<request_reply::DDSRequestReplyServer<
//         CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType, CalculatorReplyType>>(
//         SERVER_NAME, request_callback);
//   } else if (strcmp(argv[1], "cli") == 0) {
//     auto ptr2 = std::make_shared<request_reply::DDSRequestReplyClient<
//         CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType, CalculatorReplyType>>(
//         SERVER_NAME);
//     CalculatorReplyType Reply;  
//     CalculatorRequestType request;
//     request.client_id();
//     request.x(100);
//     request.y(100);
//     request.operation(CalculatorOperationType::ADDITION);
//     if (ptr2->send_request_for_wait(request, Reply, 3000)) {
//       std::cout << "send request success" << std::endl;
//       LOG(info) << "Reply received with result '" << Reply.result() << "'";
//     }
//   } else {
//     std::cerr << "unknown command: " << argv[1] << std::endl;
//   }
//   while (std::cin.get() != '\n') {
//   }
//   return 0;
// }


#include <unistd.h>

#include <iostream>
#include <thread>
#include <vector>

#include "log/logger.h"
#include "request_reply_api/DDSRequestReplyClient.h"
#include "request_reply_api/DDSRequestReplyServer.h"

#include "RequestReplyTypes/Calculator.hpp"
#include "RequestReplyTypes/CalculatorPubSubTypes.hpp"

bool reply_callback(const CalculatorReplyType &reply, const SampleInfo &info)
{
    static int count = 0;
    count++;
    LOG(info) << "+++++++++++++++++++++++Custom callback: Reply received with result '"
              << reply.result() << "'";
    if(count > 2)
    {
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
    result = request.x() + request.y();
    reply.client_id("x");
    reply.result(222222);
    LOG(info) << "----------------------[SVR]request_callback2";
    // std::this_thread::sleep_for(std::chrono::seconds(10));
};

int main(int argc, char *argv[])
{
    Logger::Instance().setFlushOnLevel(Logger::trace);
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::trace, 60, 5);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " svr/cli" << std::endl;
        return -1;
    }
    if (strcmp(argv[1], "svr") == 0) {
        auto ptr = std::make_shared<request_reply::DDSRequestReplyServer<
            CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType,
            CalculatorReplyType>>("SERVER_NAME", request_callback2);
        auto ptr2 = std::make_shared<request_reply::DDSRequestReplyClient<
            CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType,
            CalculatorReplyType>>("SERVER_NAME");
        CalculatorRequestType request;
            request.client_id();
            request.x(200);
            request.y(100);
            request.operation(CalculatorOperationType::ADDITION);
            CalculatorReplyType response;
            if (ptr2->send_request(request, reply_callback)) {
                std::cout << "send request success" << std::endl;
                std::cout << "result: " << response.result() << std::endl;
            }

        while (std::cin.get() != '\n') {
        }          
    } else if (strcmp(argv[1], "cli") == 0) {
        auto ptr3 = std::make_shared<request_reply::DDSRequestReplyServer<
        CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType,
        CalculatorReplyType>>("SERVER_NAME", request_callback);
        auto ptr4 = std::make_shared<request_reply::DDSRequestReplyClient<
            CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType,
            CalculatorReplyType>>("SERVER_NAME");
        CalculatorRequestType request;
        request.client_id();
        request.x(100);
        request.y(100);
        request.operation(CalculatorOperationType::ADDITION);

        CalculatorReplyType response;
        if (ptr4->send_request(request, reply_callback)) {
            std::cout << "send request success" << std::endl;
            std::cout << "result: " << response.result() << std::endl;
        }
        while (std::cin.get() != '\n') {
        }
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }
    return 0;
}

