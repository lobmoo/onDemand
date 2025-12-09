/**
 * @file fastdds_topic_writer.hpp
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

#ifndef DDS_TOPIC_DATAWRITER_H
#define DDS_TOPIC_DATAWRITER_H

#include "fastdds_listeners.h"
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

namespace FastddsWrapper
{
template <typename T>
class FastDDSTopicWriter
{
public:
    FastDDSTopicWriter(eprosima::fastdds::dds::Publisher *publisher,
                       eprosima::fastdds::dds::Topic *topic,
                       const eprosima::fastdds::dds::DataWriterQos &dataWriterQos,
                       DataWriterListener *listener);
    ~FastDDSTopicWriter();

public:
    bool writeMessage(const T &message) const;
    bool clear_history(size_t *removed) const;
    bool wait_for_acknowledgments(const eprosima::fastdds::dds::Duration_t &max_wait) const;
    bool get_matched_subscriptions(std::vector<InstanceHandle_t> &subscription_handles);

private:
    eprosima::fastdds::dds::DataWriter *m_dataWriter;
    eprosima::fastdds::dds::Publisher *m_publisher;
};

template <typename T>
FastDDSTopicWriter<T>::FastDDSTopicWriter(
    eprosima::fastdds::dds::Publisher *publisher, eprosima::fastdds::dds::Topic *topic,
    const eprosima::fastdds::dds::DataWriterQos &dataWriterQos, DataWriterListener *listener)
    : m_publisher(publisher)
{
    m_dataWriter = publisher->create_datawriter(topic, dataWriterQos, listener);
}

template <typename T>
FastDDSTopicWriter<T>::~FastDDSTopicWriter()
{
    if (m_dataWriter && m_publisher) {
        m_publisher->delete_datawriter(m_dataWriter);
    }
}

template <typename T>
bool FastDDSTopicWriter<T>::writeMessage(const T &message) const
{
    if (!m_dataWriter) {
        LOG(error) << "DataWriter is null, cannot write message";
        return false;
    }
    return m_dataWriter->write(const_cast<void *>(static_cast<const void *>(&message)))
           == eprosima::fastdds::dds::RETCODE_OK;
}

template <typename T>
bool FastDDSTopicWriter<T>::clear_history(size_t *removed) const
{
    if (!m_dataWriter) {
        LOG(error) << "DataWriter is null";
        return false;
    }
    return m_dataWriter->clear_history(removed) == eprosima::fastdds::dds::RETCODE_OK;
}

template <typename T>
bool FastDDSTopicWriter<T>::wait_for_acknowledgments(
    const eprosima::fastdds::dds::Duration_t &max_wait) const
{
    if (!m_dataWriter) {
        LOG(error) << "DataWriter is null";
        return false;
    }
    return m_dataWriter->wait_for_acknowledgments(max_wait) == eprosima::fastdds::dds::RETCODE_OK
               ? true
               : false;
}

template <typename T>
bool FastDDSTopicWriter<T>::get_matched_subscriptions(
    std::vector<InstanceHandle_t> &subscription_handles)
{
    if (!m_dataWriter) {
        LOG(error) << "DataWriter is null";
        return false;
    }
    return m_dataWriter->get_matched_subscriptions(subscription_handles)
                   == eprosima::fastdds::dds::RETCODE_OK
               ? true
               : false;
}
} // namespace FastddsWrapper
#endif