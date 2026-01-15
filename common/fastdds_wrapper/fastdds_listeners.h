/**
 * @file fastdds_listeners.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-11-21     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef FASTDDS_LISTENERS_H
#define FASTDDS_LISTENERS_H

#include <functional>
#include <memory>
#include <string>

#include <fastdds/dds/core/detail/DDSReturnCode.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/topic/TopicDescription.hpp>

#include "log/logger.h"

namespace FastddsWrapper
{

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

/**
 * @brief Participant发现状态
 */
enum class ParticipantStatus {
    DISCOVERED, /*发现*/
    REMOVED,    /*移除*/
    CHANGED,    /*改变*/
    DROPPED,    /*丢失*/
    IGNORE      /*忽略*/
};

/**
 * @brief 匹配信息
 */
struct MatchedInfo {
    std::string topic_name;
    int current_count;        // 当前匹配数量
    int current_count_change; // 变化量
    int total_count;          // 累计总数
};

/**
 * @brief Participant信息
 */
struct ParticipantInfo {
    std::string participant_name;
    int domain_id;
    ParticipantStatus status;
};

/**
 * @brief Endpoint发现信息
 */
struct EndpointInfo {
    std::string topic_name;
    std::string type_name;
    bool discovered; // true: discovered, false: removed
};

//  Participant监听器基类

class ParticipantListener : public DomainParticipantListener
{
public:
    ParticipantListener() = default;
    virtual ~ParticipantListener() = default;

    virtual void onParticipantDiscovery(const ParticipantInfo &info)
    {
        LOG(info) << "Participant discovered: " << info.participant_name
                  << " (Domain ID: " << info.domain_id
                  << ", Status: " << static_cast<int>(info.status) << ")";
    }
    virtual void onReaderDiscovery(const EndpointInfo &info) {}
    virtual void onWriterDiscovery(const EndpointInfo &info) {}

protected:
    void on_participant_discovery(DomainParticipant *participant, ParticipantDiscoveryStatus reason,
                                  const ParticipantBuiltinTopicData &info,
                                  bool &should_be_ignored) override
    {
        should_be_ignored = false;

        ParticipantInfo pinfo;
        pinfo.participant_name = info.participant_name.to_string();
        pinfo.domain_id = info.domain_id;

        switch (reason) {
            case ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT:
                pinfo.status = ParticipantStatus::DISCOVERED;
                break;
            case ParticipantDiscoveryStatus::REMOVED_PARTICIPANT:
                pinfo.status = ParticipantStatus::REMOVED;
                break;
            case ParticipantDiscoveryStatus::CHANGED_QOS_PARTICIPANT:
                pinfo.status = ParticipantStatus::CHANGED;
                break;
            case ParticipantDiscoveryStatus::DROPPED_PARTICIPANT:
                pinfo.status = ParticipantStatus::DROPPED;
                break;
            case ParticipantDiscoveryStatus::IGNORED_PARTICIPANT:
                pinfo.status = ParticipantStatus::IGNORE;
            default:
                return;
        }

        onParticipantDiscovery(pinfo);
    }

    void on_data_reader_discovery(DomainParticipant *participant, ReaderDiscoveryStatus reason,
                                  const SubscriptionBuiltinTopicData &info,
                                  bool &should_be_ignored) override
    {
        should_be_ignored = false;

        EndpointInfo einfo;
        einfo.topic_name = info.topic_name.to_string();
        einfo.type_name = info.type_name.to_string();
        einfo.discovered = (reason == ReaderDiscoveryStatus::DISCOVERED_READER);

        onReaderDiscovery(einfo);
    }

    void on_data_writer_discovery(DomainParticipant *participant, WriterDiscoveryStatus reason,
                                  const PublicationBuiltinTopicData &info,
                                  bool &should_be_ignored) override
    {
        should_be_ignored = false;

        EndpointInfo einfo;
        einfo.topic_name = info.topic_name.to_string();
        einfo.type_name = info.type_name.to_string();
        einfo.discovered = (reason == WriterDiscoveryStatus::DISCOVERED_WRITER);

        onWriterDiscovery(einfo);
    }
};

//  DataWriter监听器基类
class DataWriterListener : public eprosima::fastdds::dds::DataWriterListener
{
public:
    DataWriterListener() = default;
    virtual ~DataWriterListener() = default;

    virtual void onPublicationMatched(const MatchedInfo &info)
    {
        LOG(info) << "Publication matched on topic: " << info.topic_name
                  << " (Current: " << info.current_count
                  << ", Change: " << info.current_count_change << ", Total: " << info.total_count
                  << ")";
    }
    virtual void onOfferedDeadlineMissed(const std::string &topic_name) {}
    virtual void onLivelinessLost(const std::string &topic_name) {}

protected:
    void on_publication_matched(DataWriter *writer, const PublicationMatchedStatus &info) override
    {
        MatchedInfo minfo;
        minfo.topic_name = writer->get_topic()->get_name();
        minfo.current_count = info.current_count;
        minfo.current_count_change = info.current_count_change;
        minfo.total_count = info.total_count;

        onPublicationMatched(minfo);
    }

    void on_offered_deadline_missed(DataWriter *writer,
                                    const OfferedDeadlineMissedStatus &status) override
    {
        std::string topic_name = writer->get_topic()->get_name();
        LOG(warning) << "Deadline missed on topic: " << topic_name
                     << " (total: " << status.total_count << ")";
        onOfferedDeadlineMissed(topic_name);
    }

    void on_liveliness_lost(DataWriter *writer, const LivelinessLostStatus &status) override
    {
        std::string topic_name = writer->get_topic()->get_name();
        LOG(warning) << "Liveliness lost on topic: " << topic_name
                     << " (total: " << status.total_count << ")";
        onLivelinessLost(topic_name);
    }
};

template <typename T>
using MessageCallback = std::function<void(const std::string &topic, std::shared_ptr<T> message)>;

// DataReader监听器基类
template <typename T>
class DataReaderListener : public eprosima::fastdds::dds::DataReaderListener
{
public:
    DataReaderListener() = default;
    virtual ~DataReaderListener() = default;

public:
    void setMessageCallback(MessageCallback<T> callback)
    {
        message_callback_ = std::move(callback);
    }

    virtual void onSubscriptionMatched(const MatchedInfo &info)
    {
        LOG(info) << "Subscription matched on topic: " << info.topic_name
                  << " (Current: " << info.current_count
                  << ", Change: " << info.current_count_change << ", Total: " << info.total_count
                  << ")";
    }
    virtual void onRequestedDeadlineMissed(const std::string &topic_name) {}
    virtual void onLivelinessChanged(const std::string &topic_name, bool alive) {}

protected:
    MessageCallback<T> message_callback_;

protected:
    void on_subscription_matched(DataReader *reader, const SubscriptionMatchedStatus &info) override
    {
        MatchedInfo minfo;
        minfo.topic_name = reader->get_topicdescription()->get_name();
        minfo.current_count = info.current_count;
        minfo.current_count_change = info.current_count_change;
        minfo.total_count = info.total_count;

        onSubscriptionMatched(minfo);
    }

    void on_data_available(DataReader *reader) override
    {
        SampleInfo info;
        T message;
        std::string topic_name = reader->get_topicdescription()->get_name();

        // 使用完整命名空间
        while (reader->take_next_sample(&message, &info) == eprosima::fastdds::dds::RETCODE_OK) {
            if (info.valid_data) {
                try {
                    auto msg_ptr = std::make_shared<T>(message);
                    if (message_callback_) {
                        message_callback_(topic_name, msg_ptr);
                    }
                } catch (const std::exception &e) {
                    LOG(error) << "Exception in onDataAvailable for topic " << topic_name << ": "
                               << e.what();
                }
            }
        }
    }

    void on_liveliness_changed(DataReader *reader, const LivelinessChangedStatus &status) override
    {
        std::string topic_name = reader->get_topicdescription()->get_name();
        bool alive = (status.alive_count > 0);

        LOG(info) << "Liveliness changed on topic: " << topic_name
                  << " (alive: " << status.alive_count << ", not alive: " << status.not_alive_count
                  << ")";

        onLivelinessChanged(topic_name, alive);
    }

    void on_requested_deadline_missed(DataReader *reader,
                                      const RequestedDeadlineMissedStatus &status) override
    {
        std::string topic_name = reader->get_topicdescription()->get_name();
        LOG(warning) << "Requested deadline missed on topic: " << topic_name
                     << " (total: " << status.total_count << ")";
        onRequestedDeadlineMissed(topic_name);
    }
};

} // namespace FastddsWrapper

#endif // FASTDDS_LISTENERS_H