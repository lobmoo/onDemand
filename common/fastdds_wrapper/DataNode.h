#ifndef DATANODE_H
#define DATANODE_H

#include <memory>
#include <stdexcept>

#include "DDSParticipantManager.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include "log/logger.h"

class DataNode : public DDSParticipantManager {
  using ParticipantQosConfigurator = std::function<ParticipantQosHandler()>;

 public:
  DataNode(int domainId, const std::string &participant_name, ParticipantQosConfigurator qos_configurator = nullptr)
      : DDSParticipantManager(domainId), qos_configurator_(qos_configurator) {
    initDomainParticipant(participant_name);
  }

  DataNode(const std::string &qosXmlConfig)
      : DDSParticipantManager(0){
        initDomainParticipantForXml(qosXmlConfig);
  }

  ~DataNode() override {}

 private:
  mutable std::mutex topicMutex_;
  std::unordered_map<std::string, std::function<eprosima::fastdds::dds::TopicDataType *()>> topicTypeFactory_;
  ParticipantQosConfigurator qos_configurator_;

 protected:
  ParticipantQosHandler createParticipantQos(
      const std::string &participant_name, uint16_t listen_port,
      const std::vector<std::string> &peer_locators) override {
    if (nullptr != qos_configurator_) {
      return qos_configurator_();
    } else {
      ParticipantQosHandler handler(participant_name);
      return handler;
    }
  }

 public:
  template <typename T>
  void registerTopicType(const std::string &topicName) {
    addTopicDataTypeCreator(topicName, []() { return new T(); });
  }

  // 创建 DataWriter
  template <typename T>
  DDSTopicDataWriter<T> *createDataWriter(const std::string topicName) {
    return DDSParticipantManager::createDataWriter<T>(topicName);
  }

  // 创建 DataReader
  template <typename T>
  DDSTopicDataReader<T> *createDataReader(
      const std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback) {
    return DDSParticipantManager::createDataReader<T>(topicName, callback);
  }
};

#endif