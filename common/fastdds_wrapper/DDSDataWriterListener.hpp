
/**
 * @file DDSDataWriterListener.hpp
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
#ifndef DDS_DATAWRITER_LISTENER_H
#define DDS_DATAWRITER_LISTENER_H

#include <log/logger.h>

#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/Topic.hpp>

class DDSDataWriterListener : public eprosima::fastdds::dds::DataWriterListener
{
protected:
    virtual void
    on_publication_matched(eprosima::fastdds::dds::DataWriter *writer,
                           const eprosima::fastdds::dds::PublicationMatchedStatus &info) override
    {
        const eprosima::fastdds::dds::Topic *topic = writer->get_topic();

        if (info.current_count_change == 1) {
            LOG(info) << "publication " << topic->get_name() << " matched";
        } else if (info.current_count_change == -1) {
            LOG(info) << "publication " << topic->get_name() << " unmatched";
        }
    }

    virtual void on_offered_deadline_missed(
        eprosima::fastdds::dds::DataWriter *writer,
        const eprosima::fastdds::dds::OfferedDeadlineMissedStatus &status) override
    {
        LOG(info) << "on_offered_deadline_missed: " << status.total_count;
    }

    virtual void
    on_liveliness_lost(eprosima::fastdds::dds::DataWriter *writer,
                       const eprosima::fastdds::dds::LivelinessLostStatus &status) override
    {
        LOG(info) << "on_liveliness_lost: " << status.total_count;
    }
};

#endif