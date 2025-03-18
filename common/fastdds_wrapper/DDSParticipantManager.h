#ifndef DDS_PARTICIPANT_MANAGER_H
#define DDS_PARTICIPANT_MANAGER_H

#include <fastdds/dds/topic/TopicDataType.hpp>
#include <memory>

#include "DDSDomainParticipant.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include "ParticipantQosHandler.h"

class DDSParticipantManager {
  typedef std::function<eprosima::fastdds::dds::TopicDataType *()> TopicDataTypeCreator;

 public:
  DDSParticipantManager(int domainId);
  DDSParticipantManager();
  virtual ~DDSParticipantManager();

 protected:
  eprosima::fastdds::dds::TopicDataType *getTopicDataType(std::string topicName);
  void addTopicDataTypeCreator(std::string topicName, TopicDataTypeCreator creator);
  virtual ParticipantQosHandler createParticipantQos(
      const std::string &participant_name) = 0;

 public:
  bool initDomainParticipant(
      const std::string &participant_name, DomainParticipantListener* listener);
  bool initDomainParticipantForXml(const std::string &xmlConfig, DomainParticipantListener* listener);

      template <typename T>
      DDSTopicDataWriter<T> *createDataWriter(std::string topicName);

  template <typename T>
  DDSTopicDataReader<T> *createDataReader(
      std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback);

 private:
  int m_domainId;
  std::shared_ptr<DDSDomainParticipant> m_participant;

 private:
  std::unordered_map<std::string, std::function<eprosima::fastdds::dds::TopicDataType *()>> m_topicTypes;
};

template <typename T>
DDSTopicDataWriter<T> *DDSParticipantManager::createDataWriter(std::string topicName) {
  if (!m_participant->registerTopic(topicName, getTopicDataType(topicName))) return nullptr;
  return m_participant->createDataWriter<T>(topicName);
}

template <typename T>
DDSTopicDataReader<T> *DDSParticipantManager::createDataReader(
    std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback) {
  if (!m_participant->registerTopic(topicName, getTopicDataType(topicName))) return nullptr;

  return m_participant->createDataReader(topicName, callback);
}

#endif