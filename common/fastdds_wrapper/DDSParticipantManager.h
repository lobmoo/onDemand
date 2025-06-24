/**
 * @file DDSParticipantManager.h
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
#ifndef DDS_PARTICIPANT_MANAGER_H
#define DDS_PARTICIPANT_MANAGER_H

#include <fastdds/dds/topic/TopicDataType.hpp>
#include <memory>

#include "DDSDomainParticipant.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include "ParticipantQosHandler.h"
#include "DDSDataWriterQosHanler.h"
#include "DDSDataReaderQosHandler.h"

class DDSParticipantManager
{
    typedef std::function<eprosima::fastdds::dds::TopicDataType *()> TopicDataTypeCreator;

public:
    DDSParticipantManager(int domainId);
    DDSParticipantManager();
    virtual ~DDSParticipantManager();

protected:
    eprosima::fastdds::dds::TopicDataType *getTopicDataType(std::string topicName);
    void addTopicDataTypeCreator(std::string topicName, TopicDataTypeCreator creator);
    virtual ParticipantQosHandler createParticipantQos(const std::string &participant_name) = 0;
    virtual DDSDataWriterQosHanler createDataWriterQos() = 0;
    virtual DDSDataReaderQosHandler createDataReaderQos() = 0;

public:
    bool initDomainParticipant(const std::string &participant_name,
                               DomainParticipantListener *listener);
    bool initDomainParticipantForXml(const std::string &xmlConfig,
                                     DomainParticipantListener *listener);

    template <typename T>
    std::shared_ptr<DDSTopicDataWriter<T>> createDataWriter(std::string topicName);

    template <typename T>
    std::shared_ptr<DDSTopicDataReader<T>>
    createDataReader(std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<T>)> callback);

private:
    int m_domainId;
    std::shared_ptr<DDSDomainParticipant> m_participant;
    std::atomic<bool> m_isXmlConfig_;
    eprosima::fastdds::dds::TopicQos topicQos_;

private:
    std::unordered_map<std::string, std::function<eprosima::fastdds::dds::TopicDataType *()>>
        m_topicTypes;
};

template <typename T>
std::shared_ptr<DDSTopicDataWriter<T>>
DDSParticipantManager::createDataWriter(std::string topicName)
{
    eprosima::fastdds::dds::DataWriterQos m_dataWriterQos;
    if (m_isXmlConfig_) {
        if (!m_participant->get_default_datawriter_qos(m_dataWriterQos)) {
            LOG(error) << "get default datawriter qos error, using default datawriter qos";
            m_dataWriterQos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
        }

    } else {

        m_dataWriterQos = createDataWriterQos().getQos();
    }
    if (!m_participant->get_default_topic_qos(topicQos_)) {
        LOG(error) << "get default topic qos error, using default topic qos";
        topicQos_ = eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;
    }
    if (!m_participant->registerTopic(topicName, getTopicDataType(topicName), topicQos_))
        return nullptr;
    return m_participant->createDataWriter<T>(topicName, m_dataWriterQos);
}

template <typename T>
std::shared_ptr<DDSTopicDataReader<T>> DDSParticipantManager::createDataReader(
    std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback)
{
    eprosima::fastdds::dds::DataReaderQos m_dataReaderQos;
    if (m_isXmlConfig_) {
        if (!m_participant->get_default_datareader_qos(m_dataReaderQos)) {
            LOG(error) << "get default datareader qos error, using default datareader qos";
            m_dataReaderQos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
        }

    } else {

        m_dataReaderQos = createDataReaderQos().getQos();
    }

    if (!m_participant->get_default_topic_qos(topicQos_)) {
        LOG(error) << "get default topic qos error, using default topic qos";
        topicQos_ = eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;
    }
    if (!m_participant->registerTopic(topicName, getTopicDataType(topicName), topicQos_))
        return nullptr;
    return m_participant->createDataReader(topicName, callback, m_dataReaderQos);
}

#endif
