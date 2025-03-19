#ifndef REQUESTER_H
#define REQUESTER_H

#include "DDSDomainParticipant.h"
#include "DDSParticipantManager.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include "log/logger.h"

class DDSRequestReplyNode : public DDSParticipantManager {
 public:
  DDSRequestReplyNode(int domainId, const std::string &participant_name, DomainParticipantListener *listener = nullptr) : DDSParticipantManager(domainId) {
    initDomainParticipant(participant_name, listener);
  }

  ~DDSRequestReplyNode() override {}

 protected:
  ParticipantQosHandler createParticipantQos(const std::string &participant_name) override {
    ParticipantQosHandler handler(participant_name);
    return handler;
  }

 public:
  template <typename T>
  void registerTopicType(const std::string &topicName) {
    addTopicDataTypeCreator(topicName, []() { return new T(); });
  }

  template <typename T>
  DDSTopicDataWriter<T> *createDataWriter(
      const std::string topicName,
      eprosima::fastdds::dds::DataWriterQos dataWriterQos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT);

  template <typename T>
  DDSTopicDataReader<T> *createDataReader(
      const std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback,
      const eprosima::fastdds::dds::DataReaderQos &dataReaderQos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT);
};

/**
 * @brief  创建数据写入者
 * @param  topicName        topicName
 * @return DDSTopicDataWriter<T>*
 */
template <typename T>
DDSTopicDataWriter<T> *createDataWriter(
    const std::string topicName,
    eprosima::fastdds::dds::DataWriterQos dataWriterQos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT) {
  return DDSParticipantManager::createDataWriter<T>(topicName, dataWriterQos);
}

/**
 * @brief 创建数据读取者
 * @param  topicName        topicName
 * @param  callback         callback 数据接收回调
 * @return DDSTopicDataReader<T>*
 */
template <typename T>
DDSTopicDataReader<T> *createDataReader(
    const std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback,
    const eprosima::fastdds::dds::DataReaderQos &dataReaderQos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT) {
  return DDSParticipantManager::createDataReader<T>(topicName, callback, dataReaderQos);
}
#endif