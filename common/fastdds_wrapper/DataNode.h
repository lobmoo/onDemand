#ifndef DATANODE_H
#define DATANODE_H

#include <memory>
#include <stdexcept>

#include "DDSParticipantManager.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include "log/logger.h"

class DataNode : public DDSParticipantManager {
 public:
  DataNode(int domainId, const std::string &participant_name) : DDSParticipantManager(domainId) {
    initDomainParticipant(participant_name);
  }

  ~DataNode() override {}

 private:
  mutable std::mutex topicMutex_;
  std::unordered_map<std::string, std::function<eprosima::fastdds::dds::TopicDataType *()>> topicTypeFactory_;

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
  template <typename T>
  void registerTopicType(const std::string &topicName) {
    std::lock_guard<std::mutex> lock(topicMutex_);
    if (topicTypeFactory_.find(topicName) == topicTypeFactory_.end()) {
      topicTypeFactory_[topicName] = []() { return new T(); };
    }
  }

  // 创建 DataWriter
  template <typename T>
  DDSTopicDataWriter<T> *createDataWriter(const std::string topicName) {
    {
      std::lock_guard<std::mutex> lock(topicMutex_);
      auto it = topicTypeFactory_.find(topicName);
      if (it == topicTypeFactory_.end()) {
        LOG(error) << "Error: Topic type for '" << topicName << "' not registered!";
        return nullptr;
      }

      eprosima::fastdds::dds::TopicDataType *dataType = it->second();
      addTopicDataTypeCreator(topicName, [dataType]() { return dataType; });
    }
   
    return DDSParticipantManager::createDataWriter<T>(topicName);
  }

  // 创建 DataReader
  template <typename T>
  DDSTopicDataReader<T> *createDataReader(
      const std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback) {
    {
      std::lock_guard<std::mutex> lock(topicMutex_);
      auto it = topicTypeFactory_.find(topicName);
      if (it == topicTypeFactory_.end()) {
        LOG(error) << "Error: Topic type for '" << topicName << "' not registered!";
        return nullptr;
      }

      eprosima::fastdds::dds::TopicDataType *dataType = it->second();
      addTopicDataTypeCreator(topicName, [dataType]() { return dataType; });
    }
    
    return DDSParticipantManager::createDataReader<T>(topicName, callback);
  }
};

#endif