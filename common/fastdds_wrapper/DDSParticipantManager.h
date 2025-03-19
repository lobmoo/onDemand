#ifndef DDS_PARTICIPANT_MANAGER_H
#define DDS_PARTICIPANT_MANAGER_H

#include <fastdds/dds/topic/TopicDataType.hpp>
#include <memory>

#include "DDSDomainParticipant.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include "ParticipantQosHandler.h"

#define CONFIGURATION_TOPIC_PROFILE "configuration_topic_profile"
#define CONFIGURATION_DATAWRITER_PROFILE "configuration_datawriter_profile"
#define CONFIGURATION_DATAREADER_PROFILE "configuration_datareader_profile"

class DDSParticipantManager {
  typedef std::function<eprosima::fastdds::dds::TopicDataType *()> TopicDataTypeCreator;

 public:
  DDSParticipantManager(int domainId);
  DDSParticipantManager();
  virtual ~DDSParticipantManager();

 protected:
  eprosima::fastdds::dds::TopicDataType *getTopicDataType(std::string topicName);
  void addTopicDataTypeCreator(std::string topicName, TopicDataTypeCreator creator);
  virtual ParticipantQosHandler createParticipantQos(const std::string &participant_name) = 0;

 public:
  bool initDomainParticipant(const std::string &participant_name, DomainParticipantListener *listener);
  bool initDomainParticipantForXml(const std::string &xmlConfig, DomainParticipantListener *listener);

  template <typename T>
  DDSTopicDataWriter<T> *createDataWriter(
      std::string topicName, const eprosima::fastdds::dds::DataWriterQos dataWriterQos);

  template <typename T>
  DDSTopicDataReader<T> *createDataReader(
      std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback,
      const eprosima::fastdds::dds::DataReaderQos &dataReaderQos);

 private:
  int m_domainId;
  std::shared_ptr<DDSDomainParticipant> m_participant;
  std::atomic<bool> m_isXmlConfig_;
  eprosima::fastdds::dds::TopicQos topicQos_;

 private:
  std::unordered_map<std::string, std::function<eprosima::fastdds::dds::TopicDataType *()>> m_topicTypes;
};

template <typename T>
DDSTopicDataWriter<T> *DDSParticipantManager::createDataWriter(
    std::string topicName, const eprosima::fastdds::dds::DataWriterQos dataWriterQos) {
  /*配置文件的话，从配置读取*/
  if (m_isXmlConfig_) {
    if (!m_participant->get_topic_qos_from_profile(CONFIGURATION_TOPIC_PROFILE, topicQos_)) {
      LOG(error) << "get topic qos from profile error, using default topic qos";
    }
    eprosima::fastdds::dds::DataWriterQos config_dataWriterQos = dataWriterQos;
    if (!m_participant->get_datawriter_qos_from_profile(CONFIGURATION_DATAWRITER_PROFILE, config_dataWriterQos)) {
      LOG(error) << "get datawriter qos from profile error, using default datawriter qos";
    }
    if (!m_participant->registerTopic(topicName, getTopicDataType(topicName), topicQos_)) return nullptr;
    return m_participant->createDataWriter<T>(topicName, config_dataWriterQos);
  }
  if (!m_participant->registerTopic(topicName, getTopicDataType(topicName), topicQos_)) return nullptr;
  return m_participant->createDataWriter<T>(topicName, dataWriterQos);
}

template <typename T>
DDSTopicDataReader<T> *DDSParticipantManager::createDataReader(
    std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback,
    const eprosima::fastdds::dds::DataReaderQos &dataReaderQos) {
  if (m_isXmlConfig_) {
    if (!m_participant->get_topic_qos_from_profile(CONFIGURATION_TOPIC_PROFILE, topicQos_)) {
      LOG(error) << "get topic qos from profile error, using default topic qos";
    }
    eprosima::fastdds::dds::DataReaderQos config_dataReaderQos = dataReaderQos;
    if (!m_participant->get_datareader_qos_from_profile(CONFIGURATION_DATAREADER_PROFILE, config_dataReaderQos)) {
      LOG(error) << "get datareader qos from profile error, using default datareader qos";
    }
    if (!m_participant->registerTopic(topicName, getTopicDataType(topicName), topicQos_)) return nullptr;
    return m_participant->createDataReader(topicName, callback, config_dataReaderQos);
  }
  if (!m_participant->registerTopic(topicName, getTopicDataType(topicName), topicQos_)) return nullptr;
  return m_participant->createDataReader(topicName, callback, dataReaderQos);
}

#endif