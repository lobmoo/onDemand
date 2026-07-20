/**
 * @file txdds_listeners.h
 * @brief TXDDS 监听器封装，结构与 fastdds_wrapper 对齐
 */
#ifndef TXDDS_LISTENERS_H
#define TXDDS_LISTENERS_H

#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include "txdds/DCPS/domain/IDomainParticipant.h"
#include "txdds/DCPS/domain/IDomainParticipantListener.h"
#include "txdds/DCPS/publisher/IDataWriter.h"
#include "txdds/DCPS/publisher/IDataWriterListener.h"
#include "txdds/DCPS/subscriber/IDataReader.h"
#include "txdds/DCPS/subscriber/IDataReaderListener.h"
#include "txdds/DCPS/subscriber/SampleInfo.h"
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/RTPS/common/Guid.h"
#include "txdds/RTPS/participant/ParticipantDiscoveryInfo.h"
#include "txdds/RTPS/reader/ReaderDiscoveryInfo.h"
#include "txdds/RTPS/writer/WriterDiscoveryInfo.h"

#include "log/logger.h"

namespace TxddsWrapper
{

template <typename T>
using MessageCallback = std::function<void(const std::string &, std::shared_ptr<T>)>;

enum class ParticipantStatus {
    DISCOVERED, /*发现*/
    REMOVED,    /*移除*/
    CHANGED,    /*改变*/
    DROPPED,    /*丢失*/
    IGNORE      /*忽略*/
};

struct MatchedInfo {
    std::string topic_name;
    int current_count = 0;
    int current_count_change = 0;
    int total_count = 0;
};

struct ParticipantInfo {
    std::string participant_name;
    std::string guid;
    int domain_id = 0;
    ParticipantStatus status = ParticipantStatus::DISCOVERED;
};

struct EndpointInfo {
    std::string topic_name;
    std::string type_name;
    bool discovered = false;
};

class ParticipantListener : public BaoSky::dds::IDomainParticipantListener
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

    virtual void onReaderDiscovery(const EndpointInfo &) {}
    virtual void onWriterDiscovery(const EndpointInfo &) {}

protected:
    void OnParticipantDiscovery(BaoSky::dds::IDomainParticipant *participant,
                                BaoSky::rtps::ParticipantDiscoveryInfo &&info) override
    {
        ParticipantInfo pinfo;
        pinfo.participant_name = const_cast<BaoSky::rtps::DiscoveredParticipantData &>(info.info)
                                     .GetParticipantProxy()
                                     .mEntityName;
        pinfo.domain_id =
            participant != nullptr ? static_cast<int>(participant->GetDomainId()) : -1;

        /* 将 guidPrefix 转为 hex 字符串，用于区分同名 participant 的不同实例 */
        {
            std::ostringstream oss;
            oss << info.info.mParticipantGuid;
            pinfo.guid = oss.str();
        }

        switch (info.status) {
            case BaoSky::rtps::ParticipantDiscoveryInfo::DISCOVERED_PARTICIPANT:
                pinfo.status = ParticipantStatus::DISCOVERED;
                break;
            case BaoSky::rtps::ParticipantDiscoveryInfo::REMOVED_PARTICIPANT:
                pinfo.status = ParticipantStatus::REMOVED;
                break;
            case BaoSky::rtps::ParticipantDiscoveryInfo::CHANGED_QOS_PARTICIPANT:
                pinfo.status = ParticipantStatus::CHANGED;
                break;
            case BaoSky::rtps::ParticipantDiscoveryInfo::DROPPED_PARTICIPANT:
                pinfo.status = ParticipantStatus::DROPPED;
                break;
            case BaoSky::rtps::ParticipantDiscoveryInfo::IGNORED_PARTICIPANT:
                pinfo.status = ParticipantStatus::IGNORE;
                break;
            default:
                return;
        }
        onParticipantDiscovery(pinfo);
    }

    void OnReaderDiscovery(BaoSky::dds::IDomainParticipant *,
                           BaoSky::rtps::ReaderDiscoveryInfo &&info) override
    {
        EndpointInfo einfo;
        einfo.topic_name =
            const_cast<BaoSky::rtps::DiscoveredReaderData &>(info.info).GetTopicName();
        einfo.type_name = const_cast<BaoSky::rtps::DiscoveredReaderData &>(info.info).GetTypeName();
        einfo.discovered = (info.status == BaoSky::rtps::ReaderDiscoveryInfo::DISCOVERED_READER);
        onReaderDiscovery(einfo);
    }

    void OnWriterDiscovery(BaoSky::dds::IDomainParticipant *,
                           BaoSky::rtps::WriterDiscoveryInfo &&info) override
    {
        EndpointInfo einfo;
        einfo.topic_name =
            const_cast<BaoSky::rtps::DiscoveredWriterData &>(info.info).GetTopicName();
        einfo.type_name = const_cast<BaoSky::rtps::DiscoveredWriterData &>(info.info).GetTypeName();
        einfo.discovered = (info.status == BaoSky::rtps::WriterDiscoveryInfo::DISCOVERED_WRITER);
        onWriterDiscovery(einfo);
    }
};

class DataWriterListener : public BaoSky::dds::IDataWriterListener
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

    virtual void onOfferedDeadlineMissed(const std::string &) {}
    virtual void onLivelinessLost(const std::string &) {}

protected:
    void OnPublicationMatched(BaoSky::dds::IDataWriter *writer,
                              const BaoSky::dds::PublicationMatchedStatus &status) override
    {
        MatchedInfo info;
        auto topic = writer != nullptr ? writer->GetTopic() : nullptr;
        info.topic_name = topic != nullptr ? topic->GetTopicName() : "";
        info.current_count = status.current_count;
        info.current_count_change = status.current_count_change;
        info.total_count = status.total_count;
        onPublicationMatched(info);
    }
};

template <typename T>
class DataReaderListener : public BaoSky::dds::IDataReaderListener
{
public:
    DataReaderListener() = default;
    virtual ~DataReaderListener() = default;

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

    virtual void onRequestedDeadlineMissed(const std::string &) {}
    virtual void onLivelinessChanged(const std::string &, bool) {}

protected:
    void OnSubscriptionMatched(BaoSky::dds::IDataReader *reader,
                               const BaoSky::dds::SubscriptionMatchedStatus &status) override
    {
        MatchedInfo info;
        info.topic_name = reader != nullptr ? reader->GetTopicName() : "";
        info.current_count = status.current_count;
        info.current_count_change = status.current_count_change;
        info.total_count = status.total_count;
        onSubscriptionMatched(info);
    }

    void OnDataAvailable(BaoSky::dds::IDataReader *reader) override
    {
        if (!message_callback_) {
            return;
        }

        T message;
        BaoSky::dds::SampleInfo info;

        while (reader->TakeNextSample(&message, info) == BaoSky::dds::RETCODE_OK) {
            try {
                auto msg_ptr = std::make_shared<T>(message);
                message_callback_(reader->GetTopicName(), msg_ptr);
            } catch (const std::exception &e) {
                LOG(error) << "Exception in onDataAvailable for topic " << reader->GetTopicName()
                           << ": " << e.what();
            }
        }
    }

private:
    MessageCallback<T> message_callback_;
};

} // namespace TxddsWrapper

#endif // TXDDS_LISTENERS_H