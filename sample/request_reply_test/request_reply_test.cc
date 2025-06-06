#include <unistd.h>

#include <iostream>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

#include "log/logger.h"
#include "fastdds_replyrequest_wrapper/DDSRequestReplyClient.h"
#include "fastdds_replyrequest_wrapper/DDSRequestReplyServer.h"

#include "1k/Calculator_1k.hpp"
#include "1k/Calculator_1kPubSubTypes.hpp"

#include "100k/Calculator_100k.hpp"
#include "100k/Calculator_100kPubSubTypes.hpp"

#include "1M/Calculator_1M.hpp"
#include "1M/Calculator_1MPubSubTypes.hpp"

void request_callback_1k(const CalculatorRequestType_512 &request, CalculatorReplyType_512 &reply)
{
    int result = 0;
    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    LOG(warning) << "+++++++++++++++++[dealy] result recrvied whith reply: "
                 << timestamp - request.timestamp() << "ms    " << timestamp
                 << "  ::  " << request.timestamp();
    result = request.x() + request.y();
    reply.client_id("x");
    reply.timestamp(timestamp);

    std::unique_ptr<std::array<int16_t, 512>> data = std::make_unique<std::array<int16_t, 512>>();
    for (size_t i = 0; i < data->size(); ++i) {
        (*data)[i] = static_cast<int16_t>(10086); // 示例数据
    }
    reply.data(*data);
    reply.result(result);
    LOG(info) << "----------------------[SVR]request_callback";
};

void request_callback_100k(const CalculatorRequestType_51200 &request,
                           CalculatorReplyType_51200 &reply)
{
    int result = 0;
    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    LOG(warning) << "+++++++++++++++++[dealy] result recrvied whith reply: "
                 << timestamp - request.timestamp() << "ms    " << timestamp
                 << "  ::  " << request.timestamp();
    result = request.x() + request.y();
    reply.client_id("x");
    reply.timestamp(timestamp);
    std::unique_ptr<std::array<int16_t, 51200>> data = std::make_unique<std::array<int16_t, 51200>>();
    for (size_t i = 0; i < data->size(); ++i) {
        (*data)[i] = static_cast<int16_t>(10086); // 示例数据
    }
    reply.data(*data);
    reply.result(result);
    LOG(info) << "----------------------[SVR]request_callback";
};

void request_callback_1M(const CalculatorRequestType_524288 &request,
                         CalculatorReplyType_524288 &reply)
{
    int result = 0;
    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    LOG(warning) << "+++++++++++++++++[dealy] result recrvied whith reply: "
                 << timestamp - request.timestamp() << "ms    " << timestamp
                 << "  ::  " << request.timestamp();
    result = request.x() + request.y();
    reply.client_id("x");
    reply.timestamp(timestamp);
    std::unique_ptr<std::array<int16_t, 524288>> data = std::make_unique<std::array<int16_t, 524288>>();
    for (size_t i = 0; i < data->size(); ++i) {
        (*data)[i] = static_cast<int16_t>(10086); // 示例数据
    }
    reply.data(*data);
    reply.result(result);
    LOG(info) << "----------------------[SVR]request_callback";
};

class UDPTestRequestReply : public ::testing::Test
{

private:
    std::shared_ptr<request_reply::DDSRequestReplyServer<
        CalculatorRequestType_512PubSubType, CalculatorReplyType_512PubSubType,
        CalculatorRequestType_512, CalculatorReplyType_512>>
        server_512;
    std::shared_ptr<request_reply::DDSRequestReplyServer<
        CalculatorRequestType_51200PubSubType, CalculatorReplyType_51200PubSubType,
        CalculatorRequestType_51200, CalculatorReplyType_51200>>
        server_51200;
    std::shared_ptr<request_reply::DDSRequestReplyServer<
        CalculatorRequestType_524288PubSubType, CalculatorReplyType_524288PubSubType,
        CalculatorRequestType_524288, CalculatorReplyType_524288>>
        server_524288;

    // std::shared_ptr<request_reply::DDSRequestReplyClient<
    //     CalculatorRequestType_512PubSubType, CalculatorReplyType_512PubSubType,
    //     CalculatorRequestType_512, CalculatorReplyType_512>>
    //     client_512;
    // std::shared_ptr<request_reply::DDSRequestReplyClient<
    //     CalculatorRequestType_51200PubSubType, CalculatorReplyType_51200PubSubType,
    //     CalculatorRequestType_51200, CalculatorReplyType_51200>>
    //     client_51200;
    // std::shared_ptr<request_reply::DDSRequestReplyClient<
    //     CalculatorRequestType_524288PubSubType, CalculatorReplyType_524288PubSubType,
    //     CalculatorRequestType_524288, CalculatorReplyType_524288>>
    //     client_524288;

protected:
    void SetUp() override
    {

        server_512 = std::make_shared<request_reply::DDSRequestReplyServer<
            CalculatorRequestType_512PubSubType, CalculatorReplyType_512PubSubType,
            CalculatorRequestType_512, CalculatorReplyType_512>>("SERVER_NAME",
                                                                 request_callback_1k);

        server_51200 = std::make_shared<request_reply::DDSRequestReplyServer<
            CalculatorRequestType_51200PubSubType, CalculatorReplyType_51200PubSubType,
            CalculatorRequestType_51200, CalculatorReplyType_51200>>("SERVER_NAME",
                                                                     request_callback_100k);

        server_524288 = std::make_shared<request_reply::DDSRequestReplyServer<
            CalculatorRequestType_524288PubSubType, CalculatorReplyType_524288PubSubType,
            CalculatorRequestType_524288, CalculatorReplyType_524288>>("SERVER_NAME",
                                                                       request_callback_1M);
    };

    void TearDown() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        server_512.reset();
        server_51200.reset();
        server_524288.reset();
    }
};

bool reply_callback_1k(const CalculatorReplyType_512 &reply, const SampleInfo &info)
{

    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    LOG(warning) << "+++++++++++++++++[dealy] Reply recrvied whith result: "
                 << timestamp - reply.timestamp() << "ms";
    static int count = 0;
    count++;
    LOG(info) << "+++++++++++++++++++++++Custom callback: Reply received with result '"
              << reply.result() << "'";
    if (count > 1000) {
        return true;
    }
    return false;
};

TEST_F(UDPTestRequestReply, UDPTestRequestReplyClientTest1k)
{
    auto client_512 = std::make_shared<request_reply::DDSRequestReplyClient<
        CalculatorRequestType_512PubSubType, CalculatorReplyType_512PubSubType,
        CalculatorRequestType_512, CalculatorReplyType_512>>("SERVER_NAME");
    short index = 0;
    CalculatorRequestType_512 request;
    while (index < 100) {

        std::unique_ptr<std::array<int16_t, 512>> data =
            std::make_unique<std::array<int16_t, 512>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(index); // 示例数据
        }

        request.client_id();
        request.x(index);
        request.y(100);
        request.data(*data);
        request.operation(CalculatorOperationType_512::ADDITION);
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        request.timestamp(timestamp);
        CalculatorReplyType_512 response;
        if (!client_512->send_request(request, reply_callback_1k)) {
            LOG(error) << "send request failed";
        }
        index++;
    }

    sleep(2);
}

bool reply_callback_100k(const CalculatorReplyType_51200 &reply, const SampleInfo &info)
{

    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    LOG(warning) << "+++++++++++++++++[dealy] Reply recrvied whith result: "
                 << timestamp - reply.timestamp() << "ms";
    static int count = 0;
    count++;
    LOG(info) << "+++++++++++++++++++++++Custom callback: Reply received with result '"
              << reply.result() << "'";
    if (count > 1000) {
        return true;
    }
    return false;
};

TEST_F(UDPTestRequestReply, UDPTestRequestReplyClientTest100k)
{
    auto client_51200 = std::make_shared<request_reply::DDSRequestReplyClient<
        CalculatorRequestType_51200PubSubType, CalculatorReplyType_51200PubSubType,
        CalculatorRequestType_51200, CalculatorReplyType_51200>>("SERVER_NAME");
    short index = 0;
    CalculatorRequestType_51200 request;
    while (index < 100) {

        std::unique_ptr<std::array<int16_t, 51200>> data =
            std::make_unique<std::array<int16_t, 51200>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(index); // 示例数据
        }

        request.client_id();
        request.x(index);
        request.y(100);
        request.data(*data);
        request.operation(CalculatorOperationType_51200::ADDITION);
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        request.timestamp(timestamp);
        CalculatorReplyType_51200 response;
        if (!client_51200->send_request(request, reply_callback_100k)) {
            LOG(error) << "send request failed";
        }
        index++;
    }
    sleep(2);
}

bool reply_callback_1M(const CalculatorReplyType_524288 &reply, const SampleInfo &info)
{

    auto now = std::chrono::system_clock::now();
    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = value.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    LOG(warning) << "+++++++++++++++++[dealy] Reply recrvied whith result: "
                 << timestamp - reply.timestamp() << "ms";
    static int count = 0;
    count++;
    LOG(info) << "+++++++++++++++++++++++Custom callback: Reply received with result '"
              << reply.result() << "'";
    if (count > 1000) {
        return true;
    }
    return false;
};

TEST_F(UDPTestRequestReply, UDPTestRequestReplyClientTest1M)
{
    auto client_524288 = std::make_shared<request_reply::DDSRequestReplyClient<
        CalculatorRequestType_524288PubSubType, CalculatorReplyType_524288PubSubType,
        CalculatorRequestType_524288, CalculatorReplyType_524288>>("SERVER_NAME");
    short index = 0;
    CalculatorRequestType_524288 request;
    while (index < 100) {

        std::unique_ptr<std::array<int16_t, 524288>> data =
            std::make_unique<std::array<int16_t, 524288>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(index); // 示例数据
        }

        request.client_id();
        request.x(index);
        request.y(100);
        request.data(*data);
        request.operation(CalculatorOperationType_524288::ADDITION);
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        request.timestamp(timestamp);
        CalculatorReplyType_524288 response;
        if (!client_524288->send_request(request, reply_callback_1M)) {
            LOG(error) << "send request failed";
        }
        index++;
    }

    sleep(2);
}
