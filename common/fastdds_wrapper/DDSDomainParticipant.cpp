/**
 * @file DDSDomainParticipant.cpp
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
#include "DDSDomainParticipant.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <mutex>

using namespace eprosima::fastdds::dds;

DDSDomainParticipant::DDSDomainParticipant(
    int domainId, const eprosima::fastdds::dds::DomainParticipantExtendedQos &participantQos,
    DomainParticipantListener *listener)
{
    // 这里一定要注意，participant  设置listener之后，StatusMask必须要是none  具体可见
    // https://github.com/eProsima/Fast-DDS/issues/1902
    m_participant = DomainParticipantFactory::get_instance()->create_participant(
        domainId, participantQos, listener, StatusMask::none());
    if (m_participant) {
        eprosima::fastdds::dds::SubscriberQos subscriberQos(SUBSCRIBER_QOS_DEFAULT);
        m_subscriber = m_participant->create_subscriber(subscriberQos, nullptr);

        eprosima::fastdds::dds::PublisherQos publisherQos(PUBLISHER_QOS_DEFAULT);
        m_publisher = m_participant->create_publisher(publisherQos, nullptr);
    }
}

DDSDomainParticipant::DDSDomainParticipant(std::string XmlConfig,
                                           DomainParticipantListener *listener)
{
    if (XmlConfig.empty()) {
        LOG(warning) << "XmlConfig is empty, use default config";
        eprosima::fastdds::dds::DomainParticipantQos participantQos = PARTICIPANT_QOS_DEFAULT;
        m_participant = DomainParticipantFactory::get_instance()->create_participant(
            0, participantQos, listener, StatusMask::none());
        if (m_participant) {
            eprosima::fastdds::dds::SubscriberQos subscriberQos(SUBSCRIBER_QOS_DEFAULT);
            m_subscriber = m_participant->create_subscriber(subscriberQos, nullptr);

            eprosima::fastdds::dds::PublisherQos publisherQos(PUBLISHER_QOS_DEFAULT);
            m_publisher = m_participant->create_publisher(publisherQos, nullptr);
        }
    } else {
        // DomainParticipantFactory::get_instance()->load_XML_profiles_file(XmlConfig);
        // m_participant = DomainParticipantFactory::get_instance()->create_participant_with_profile(
        //     "configuration_participant_profile", listener, StatusMask::none());
        eprosima::fastdds::dds::DomainParticipantQos participantQos = PARTICIPANT_QOS_DEFAULT;
        DomainParticipantFactory::get_instance()->get_default_participant_qos(participantQos);
        m_participant = DomainParticipantFactory::get_instance()->create_participant(
            0, participantQos, listener, StatusMask::none());
        if (m_participant) {
            SubscriberQos sub_qos = SUBSCRIBER_QOS_DEFAULT;
            m_participant->get_default_subscriber_qos(sub_qos);
            // m_participant->get_subscriber_qos_from_profile("defaule_subscriber_profile",
            //                                                sub_qos);
            m_subscriber = m_participant->create_subscriber(sub_qos, nullptr);

            PublisherQos pub_qos = PUBLISHER_QOS_DEFAULT;
            m_participant->get_default_publisher_qos(pub_qos);
            // m_participant->get_publisher_qos_from_profile("configuration_publisher_profile",
            //                                               pub_qos);
            m_publisher = m_participant->create_publisher(pub_qos, nullptr);
        }
    }
}

DDSDomainParticipant::~DDSDomainParticipant()
{
    if (m_participant) {
        DomainParticipantFactory::get_instance()->delete_participant(m_participant);
    }
}

bool DDSDomainParticipant::registerTopic(std::string topicName,
                                         eprosima::fastdds::dds::TopicDataType *dataType,
                                         const TopicQos &topicQos)
{
    if (!dataType)
        return false;

    TypeSupport typeSupport(dataType);
    std::lock_guard<std::mutex> guard(m_topicLock);
    if (m_mapTopics.find(topicName) != m_mapTopics.end())
        return true;

    eprosima::fastdds::dds::ReturnCode_t errCode = typeSupport.register_type(m_participant);
    if (errCode != eprosima::fastdds::dds::RETCODE_OK) {
        LOG(error) << "register_type failed";
        return false;
    }
    Topic *topic = m_participant->create_topic(topicName, typeSupport.get_type_name(), topicQos);
    if (topic != nullptr)
        m_mapTopics.insert(std::pair<std::string, Topic *>(topicName, topic));
    return true;
}

bool DDSDomainParticipant::unregisterTopic(std::string topicName)
{
    std::lock_guard<std::mutex> guard(m_topicLock);
    if (m_mapTopics.find(topicName) == m_mapTopics.end())
        return true;

    Topic *topic = m_mapTopics.at(topicName);
    if (topic) {
        eprosima::fastdds::dds::ReturnCode_t errCode = m_participant->delete_topic(topic);
        if (errCode != eprosima::fastdds::dds::RETCODE_OK) {
            LOG(error) << "delete_topic " << topic->get_name() << " error, errcode: " << errCode;
            return false;
        }
    }
    m_mapTopics.erase(topicName);
    return true;
}

bool DDSDomainParticipant::get_topic_qos_from_profile(const std::string &profile_name,
                                                      TopicQos &topicQos)
{
    if (m_participant) {
        return m_participant->get_topic_qos_from_profile(profile_name, topicQos)
                       == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}

bool DDSDomainParticipant::get_datareader_qos_from_profile(const std::string &profile_name,
                                                           DataReaderQos &dataReaderQos)
{
    if (m_subscriber) {
        return m_subscriber->get_datareader_qos_from_profile(profile_name, dataReaderQos)
                       == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}

bool DDSDomainParticipant::get_datawriter_qos_from_profile(const std::string &profile_name,
                                                           DataWriterQos &dataWriterQos)
{
    if (m_publisher) {
        return m_publisher->get_datawriter_qos_from_profile(profile_name, dataWriterQos)
                       == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}

bool DDSDomainParticipant::get_default_datawriter_qos(DataWriterQos &writer_qos)
{
    if (m_publisher) {
        return m_publisher->get_default_datawriter_qos(writer_qos)
                       == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}

bool DDSDomainParticipant::get_default_datareader_qos(DataReaderQos &reader_qos)
{
    if (m_subscriber) {
        return m_subscriber->get_default_datareader_qos(reader_qos)
                       == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}

bool DDSDomainParticipant::get_default_topic_qos(TopicQos &topic_qos)
{
    if (m_participant) {
        return m_participant->get_default_topic_qos(topic_qos) == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}


// bool DDSDomainParticipant::get_default_participant_qos(DomainParticipantQos &participant_qos)
// {
//     if (m_participant) {
//         return m_participant->get_default_participant_qos(participant_qos)
//                        == eprosima::fastdds::dds::RETCODE_OK
//                    ? true
//                    : false;
//     }
//     return false;
// }

bool DDSDomainParticipant::get_default_subscriber_qos(SubscriberQos &subscriber_qos)
{
    if (m_participant) {
        return m_participant->get_default_subscriber_qos(subscriber_qos)
                       == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}
bool DDSDomainParticipant::get_default_publisher_qos(PublisherQos &publisher_qos)
{
    if (m_participant) {
        return m_participant->get_default_publisher_qos(publisher_qos)
                       == eprosima::fastdds::dds::RETCODE_OK
                   ? true
                   : false;
    }
    return false;
}
