/**
 * @file DDSParticipantListener.h
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
#ifndef DDS_PARTICIPANT_LISTENER_H
#define DDS_PARTICIPANT_LISTENER_H

#include <log/logger.h>

#include <fastdds/dds/core/detail/DDSReturnCode.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/topic/TopicDescription.hpp>

using namespace eprosima::fastdds::dds;

class DDSParticipantListener : public eprosima::fastdds::dds::DomainParticipantListener
{
public:
    // 当一个新的参与者被发现时调用
    virtual void on_participant_discovery(
        DomainParticipant *participant, eprosima::fastdds::rtps::ParticipantDiscoveryStatus reason,
        const ParticipantBuiltinTopicData &info, bool &should_be_ignored) override
    {
        LOG(info) << "on_participant_discovery";
        LOG(info) << "info.participant_name: " << info.participant_name;
        LOG(info) << "info.domain_id: " << info.domain_id;

        should_be_ignored = false;
    }

    // 当数据读取者被发现时调用
    virtual void
    on_data_reader_discovery(eprosima::fastdds::dds::DomainParticipant *participant,
                             eprosima::fastdds::rtps::ReaderDiscoveryStatus reason,
                             const eprosima::fastdds::dds::SubscriptionBuiltinTopicData &info,
                             bool &should_be_ignored) override
    {
        LOG(info) << "on_data_reader_discovery";
        should_be_ignored = false;
    }

    // 另一个 on_data_writer_discovery 的重载版本
    virtual void
    on_data_writer_discovery(eprosima::fastdds::dds::DomainParticipant *participant,
                             eprosima::fastdds::rtps::WriterDiscoveryStatus reason,
                             const eprosima::fastdds::dds::PublicationBuiltinTopicData &info,
                             bool &should_be_ignored) override
    {
        LOG(info) << "on_data_writer_discovery";
        should_be_ignored = false;
    }
};

#endif // DDS_PARTICIPANT_LISTENER_H