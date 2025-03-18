#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

#include "HelloWorldOne.hpp"
#include "HelloWorldOnePubSubTypes.hpp"
#include "fastdds_wrapper/DataNode.h"
#include "fastdds_wrapper/DDSParticipantListener.h"
#include "log/logger.h"
using namespace std;

void run_dds_data_writer();
void run_dds_data_reader();

ParticipantQosHandler qos_configurator() {
  ParticipantQosHandler handler("test");
  handler.addUDPV4Transport(32 * 1024 * 1024);
  handler.addSHMTransport(32 * 1024 * 1024);
  handler.setParticipantQosProperties("dsfcversion", "v1.0.0", true);
  return handler;
}

void processHelloWorldOne(const std::string &topic_name, std::shared_ptr<HelloWorldOne> data) {
  LOG(info) << "recv message [" << topic_name << "]: " << data->index();
}

void test_singale_sub_pub() {
  std::thread writer_thread(run_dds_data_writer);
  writer_thread.detach();
  std::thread reader_thread(run_dds_data_reader);
  reader_thread.detach();

  while (std::cin.get() != '\n') {
  }
  return;
}

void test_multi_sub_pub(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " sub/pub" << std::endl;
    return;
  }
  if (strcmp(argv[1], "sub") == 0) {
    run_dds_data_reader();
  } else if (strcmp(argv[1], "pub") == 0) {
    run_dds_data_writer();
  } else {
    std::cerr << "unknown command: " << argv[1] << std::endl;
  }
  while (std::cin.get() != '\n') {
  }
  return;
}



void test_singal_node_sub_pub() {

  DDSParticipantListener listener;
  DataNode node(170, "test_writer", qos_configurator, &listener);
  //DataNode node("/home/wwk/workspaces/test_demo/sample/node_example/qosConfig.xml");
  node.registerTopicType<HelloWorldOnePubSubType>("wwk");
  auto dataWriter = node.createDataWriter<HelloWorldOne>("wwk");
  auto dataReader = node.createDataReader<HelloWorldOne>("wwk", processHelloWorldOne);
  bool runFlag = true;
  int index = 0;
  std::thread([&]() {
    while (std::cin.get() != '\n') {
    }
    runFlag = false;
  }).detach();

  while (runFlag) {
    HelloWorldOne message;
    message.index(++index);
    message.points(std::vector<uint8_t>(100));
    if (dataWriter->writeMessage(message)) {
      LOG(info) << "send message: " << message.index();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  while (std::cin.get() != '\n') {
  }
  return;
}

int main(int argc, char *argv[]) {
  Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
  //test_singale_sub_pub();
  test_multi_sub_pub(argc, argv);
  //test_singal_node_sub_pub();
}

void run_dds_data_writer() {
  DDSParticipantListener *listener = new DDSParticipantListener();
  DataNode node(170, "test_writer", NULL, listener);
  //DataNode node(170, "test_writer");
  node.registerTopicType<HelloWorldOnePubSubType>("wwk");
  auto dataWriter = node.createDataWriter<HelloWorldOne>("wwk");
  bool runFlag = true;
  int index = 0;
  std::thread([&]() {
    while (std::cin.get() != '\n') {
    }
    runFlag = false;
  }).detach();

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

void run_dds_data_reader() {
  DDSParticipantListener *listener = new DDSParticipantListener();
  DataNode node(170, "test_reader", NULL, listener);
 // DataNode node(170, "test_reader");
  node.registerTopicType<HelloWorldOnePubSubType>("wwk");
  auto dataReader = node.createDataReader<HelloWorldOne>("wwk", processHelloWorldOne);
  while (std::cin.get() != '\n') {
  }
}
