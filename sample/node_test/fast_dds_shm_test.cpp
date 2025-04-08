#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <gtest/gtest.h>



#include "TestMessage_1k/TestMessage.hpp"
#include "TestMessage_1k/TestMessagePubSubTypes.hpp"

#include "TestMessage_100k/TestMessage100k.hpp"
#include "TestMessage_100k/TestMessage100kPubSubTypes.hpp"

#include "TestMessage_1M/TestMessage1M.hpp"
#include "TestMessage_1M/TestMessage1MPubSubTypes.hpp"

#include "TestMessage_5M/TestMessage5M.hpp"
#include "TestMessage_5M/TestMessage5MPubSubTypes.hpp"

#include "TestMessage_100M/TestMessage100M.hpp"
#include "TestMessage_100M/TestMessage100MPubSubTypes.hpp"

#include "fastdds_wrapper/DataNode.h"
#include "fastdds_wrapper/DDSParticipantListener.h"
#include "log/logger.h"
using namespace std;


TEST(SHMTest, singalTest_1k) 
{
    uint32_t cnt = 0;
    int index = 0;
    std::vector<uint64_t> delays;
    DataNode node(170, "test_node");
    node.registerTopicType<Message_512PubSubType>("Message_512");
    auto dataWriter = node.createDataWriter<Message_512>("Message_512");
    auto dataReader = node.createDataReader<Message_512>("Message_512", [&delays](const std::string &topic_name, std::shared_ptr<Message_512> data) {
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        int64_t delay_time =  timestamp - data->timestamp();
        delays.push_back(delay_time);
        LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
    });

    while (cnt < 10)
    {
        Message_512 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(index++);
        std::array<int16_t, 512> data;
        
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(data);
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } 

    uint64_t sum = 0;
    for(auto delay : delays)
    {
        sum += delay;
    }
    LOG(info) << "1K==========delay: " << sum/10;
}  



TEST(SHMTest, singalTest_100k) 
{
    uint32_t cnt = 0;
    int index = 0;
    std::vector<uint64_t> delays;
    DataNode node(170, "test_node");
    node.registerTopicType<Message_51200PubSubType>("Message_51200");
    auto dataWriter = node.createDataWriter<Message_51200>("Message_51200");
    auto dataReader = node.createDataReader<Message_51200>("Message_51200", [&delays](const std::string &topic_name, std::shared_ptr<Message_51200> data) {
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        int64_t delay_time =  timestamp - data->timestamp();
        delays.push_back(delay_time);
        LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
    });

    while (cnt < 10)
    {
        Message_51200 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(index++);
        std::unique_ptr<std::array<int16_t, 51200>> data = std::make_unique<std::array<int16_t, 51200>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(*data);
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } 

    uint64_t sum = 0;
    for(auto delay : delays)
    {
        sum += delay;
    }
    LOG(info) << "100k==========delay: " << sum/10;
}  


TEST(SHMTest, singalTest_1M) 
{
    uint32_t cnt = 0;
    int index = 0;
    std::vector<uint64_t> delays;
    DataNode node(170, "test_node");
    node.registerTopicType<Message_524288PubSubType>("Message_524288");
    auto dataWriter = node.createDataWriter<Message_524288>("Message_524288");
    auto dataReader = node.createDataReader<Message_524288>("Message_524288", [&delays](const std::string &topic_name, std::shared_ptr<Message_524288> data) {
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        int64_t delay_time =  timestamp - data->timestamp();
        delays.push_back(delay_time);
        LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
    });

    while (cnt < 10)
    {
        Message_524288 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(index++);
        std::unique_ptr<std::array<int16_t, 524288>> data = std::make_unique<std::array<int16_t, 524288>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(*data);
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } 

    uint64_t sum = 0;
    for(auto delay : delays)
    {
        sum += delay;
    }
    LOG(info) << "1M==========delay: " << sum/10;
}  


TEST(SHMTest, singalTest_5M) 
{
    uint32_t cnt = 0;
    int index = 0;
    std::vector<uint64_t> delays;
    DataNode node(170, "test_node");
    node.registerTopicType<Message_2621440PubSubType>("Message_2621440");
    auto dataWriter = node.createDataWriter<Message_2621440>("Message_2621440");
    auto dataReader = node.createDataReader<Message_2621440>("Message_2621440", [&delays](const std::string &topic_name, std::shared_ptr<Message_2621440> data) {
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        int64_t delay_time =  timestamp - data->timestamp();
        delays.push_back(delay_time);
        LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
    });

    while (cnt < 10)
    {
        Message_2621440 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(index++);
        std::unique_ptr<std::array<int16_t, 2621440>> data = std::make_unique<std::array<int16_t, 2621440>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(*data);
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } 

    uint64_t sum = 0;
    for(auto delay : delays)
    {
        sum += delay;
    }
    LOG(info) << "5M==========delay: " << sum/10;
}  



TEST(SHMTest, moreTopicTest_1k) 
{
    uint32_t cnt = 0;
    int index = 0;
    std::vector<uint64_t> delays;
    DataNode node(170, "test_node");
    node.registerTopicType<Message_512PubSubType>("Message_512_1");
    node.registerTopicType<Message_512PubSubType>("Message_512_2");
    node.registerTopicType<Message_512PubSubType>("Message_512_3");

    auto dataWriter = node.createDataWriter<Message_512>("Message_512_1");
    auto dataWriter2 = node.createDataWriter<Message_512>("Message_512_2");
    auto dataWriter3 = node.createDataWriter<Message_512>("Message_512_3");

    auto dataReader = node.createDataReader<Message_512>("Message_512", [&delays](const std::string &topic_name, std::shared_ptr<Message_512> data) {
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        int64_t delay_time =  timestamp - data->timestamp();
        delays.push_back(delay_time);
        LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
    });

    auto dataReader2 = node.createDataReader<Message_512>("Message_512_2", [&delays](const std::string &topic_name, std::shared_ptr<Message_512> data) {
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        int64_t delay_time =  timestamp - data->timestamp();
        delays.push_back(delay_time);
        LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
    });

    auto dataReader3 = node.createDataReader<Message_512>("Message_512_3", [&delays](const std::string &topic_name, std::shared_ptr<Message_512> data) {
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        int64_t delay_time =  timestamp - data->timestamp();
        delays.push_back(delay_time);
        LOG(info) << "recv message [" << topic_name << "]: " << data->id() << "recv message delay time: " << delay_time;
    });

    while (cnt < 10)
    {
        Message_512 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(index++);
        std::array<int16_t, 512> data;
        
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<int16_t>(i); // 示例数据
        }
        message.data(data);
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        if (dataWriter2->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        if (dataWriter3->writeMessage(message)) {
            LOG(info) << "send message: " << message.id();
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } 

    uint64_t sum = 0;
    for(auto delay : delays)
    {
        sum += delay;
    }
    LOG(info) << "1K==========delay: " << sum/10;
}  
