#include <cstring>
#include <iostream>
#include <memory>

#include "DDSConstants.h"
#include "fastdds_wrapper/DataNode.h"
#include "HelloWorldOnePubSubTypes.hpp"
#include "HelloWorldOne.hpp"
#include <thread>

#include "log/logger.h"
using namespace std;

void run_dds_data_writer();
void run_dds_data_reader();

int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    std::thread writer_thread(run_dds_data_writer);
    writer_thread.detach();
    std::thread reader_thread(run_dds_data_reader);
    reader_thread.detach();
 
    while (std::cin.get() != '\n') {
    }
    return 0;
}

void processHelloWorldOne(const std::string &topic_name, std::shared_ptr<HelloWorldOne> data)
{
    LOG(info) << "recv message [" << topic_name << "]: " << data->index();
}



void run_dds_data_writer()
{
    eprosima::fastdds::dds::TopicDataType *type = new HelloWorldOnePubSubType(); 
    DataNode node(170, "test_writer", type);
    auto dataWriter =  node.createDataWriter<HelloWorldOne>("wwk");
    bool runFlag = true;
    int  index = 0;
    while (runFlag) {
        HelloWorldOne message;
        message.index(++index);
        message.points(std::vector<uint8_t>(100));
        if (dataWriter->writeMessage(message)) {
            LOG(info) << "send message: " << message.index();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void run_dds_data_reader()
{   
    eprosima::fastdds::dds::TopicDataType *type = new HelloWorldOnePubSubType(); 
    DataNode node(170, "test_reader", type);
    auto dataReader =  node.createDataReader<HelloWorldOne>("wwk", processHelloWorldOne);   
}

