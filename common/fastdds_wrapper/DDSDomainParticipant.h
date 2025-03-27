/**
 * @file DDSDomainParticipant.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-03-25
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-03-25     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef DDS_DOMAIN_PARTICIPANT_H
#define DDS_DOMAIN_PARTICIPANT_H

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantExtendedQos.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <mutex>
#include <unordered_map>

#include "DDSParticipantListener.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"

class DDSDomainParticipant
{
public:
    DDSDomainParticipant(int domainId,
                         const eprosima::fastdds::dds::DomainParticipantExtendedQos &participantQos,
                         DomainParticipantListener *listener);
    DDSDomainParticipant(std::string XmlConfig, DomainParticipantListener *listener);
    virtual ~DDSDomainParticipant();

public:
    template <typename T>
    DDSTopicDataWriter<T> *createDataWriter(std::string topicName,
                                            eprosima::fastdds::dds::DataWriterQos dataWriterQos);

    template <typename T>
    DDSTopicDataReader<T> *
    createDataReader(std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<T>)> callback,
                     const eprosima::fastdds::dds::DataReaderQos &dataReaderQos);

    bool registerTopic(std::string topicName, eprosima::fastdds::dds::TopicDataType *dataType,
                       const eprosima::fastdds::dds::TopicQos &topicQos =
                           eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);

    bool unregisterTopic(std::string topicName);

    bool get_topic_qos_from_profile(const std::string &profile_name, TopicQos &topicQos);
    bool get_datareader_qos_from_profile(const std::string &profile_name,
                                         DataReaderQos &dataReaderQos);
    bool get_datawriter_qos_from_profile(const std::string &profile_name,
                                         DataWriterQos &dataWriterQos);

private:
    eprosima::fastdds::dds::DomainParticipant *m_participant;
    eprosima::fastdds::dds::Subscriber *m_subscriber;
    eprosima::fastdds::dds::Publisher *m_publisher;
    std::unordered_map<std::string, eprosima::fastdds::dds::Topic *> m_mapTopics;
    std::mutex m_topicLock;
};

template <typename T>
DDSTopicDataWriter<T> *
DDSDomainParticipant::createDataWriter(std::string topicName,
                                       eprosima::fastdds::dds::DataWriterQos dataWriterQos)
{
    std::lock_guard<std::mutex> guard(m_topicLock);
    if (m_mapTopics.find(topicName) == m_mapTopics.end())
        return nullptr;
    return new DDSTopicDataWriter<T>(m_publisher, m_mapTopics.at(topicName), dataWriterQos);
}

template <typename T>
DDSTopicDataReader<T> *DDSDomainParticipant::createDataReader(
    std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback,
    const eprosima::fastdds::dds::DataReaderQos &dataReaderQos)
{
    std::lock_guard<std::mutex> guard(m_topicLock);
    if (m_mapTopics.find(topicName) == m_mapTopics.end())
        return nullptr;
    return new DDSTopicDataReader<T>(m_subscriber, m_mapTopics.at(topicName), callback,
                                     dataReaderQos);
}

#endif