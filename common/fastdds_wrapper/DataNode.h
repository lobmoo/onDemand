#ifndef DATANODE_H
#define DATANODE_H

#include <memory>
#include <stdexcept>

#include "DDSParticipantManager.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"

class DataNode : public DDSParticipantManager {
 public:
  DataNode(int domainId, const std::string &participant_name, eprosima::fastdds::dds::TopicDataType *dataType) : DDSParticipantManager(domainId), dataType_(dataType) {
    initDomainParticipant(participant_name);
  }

  ~DataNode() override {}

  private:
    eprosima::fastdds::dds::TopicDataType *dataType_;

 protected:
  ParticipantQosHandler createParticipantQos(
      const std::string &participant_name, uint16_t listen_port,
      const std::vector<std::string> &peer_locators) override {
    ParticipantQosHandler handler(participant_name);

    handler.addSHMTransport(1024 * 1024 * 16);
    handler.addUDPV4Transport(1024 * 1024 * 16);

    return handler;
  }

 public:
  // 创建 DataWriter
  template <typename T>
  DDSTopicDataWriter<T> *createDataWriter(const std::string topicName) {
    addTopicDataTypeCreator(topicName, [this]() { return dataType_; });
    return DDSParticipantManager::createDataWriter<T>(topicName);
  }

  // 创建 DataReader
  template <typename T>
  DDSTopicDataReader<T> *createDataReader(
      const std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback) {
    addTopicDataTypeCreator(topicName, [this]() { return dataType_; });
    return DDSParticipantManager::createDataReader<T>(topicName, callback);
  }
};

#endif