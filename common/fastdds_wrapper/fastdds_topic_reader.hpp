/**
 * @file fastdds_topic_reader.hpp
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

#ifndef FASTDDS_TOPIC_READER_HPP
#define FASTDDS_TOPIC_READER_HPP

#include "fastdds_listeners.h"
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>

namespace FastddsWrapper
{
template <typename T>
class FastDDSTopicReader
{
    typedef std::function<void(const std::string &, std::shared_ptr<T>)> OnMessageCallback;

public:
    FastDDSTopicReader(eprosima::fastdds::dds::Subscriber *subscriber,
                       eprosima::fastdds::dds::Topic *topic, OnMessageCallback callback,
                       const eprosima::fastdds::dds::DataReaderQos &dataReaderQos,
                       DataReaderListener<T> *listener);

    ~FastDDSTopicReader();
    FastDDSTopicReader(const FastDDSTopicReader &) = delete;
    FastDDSTopicReader(FastDDSTopicReader &&) = delete;

private:
    DataReaderListener<T> m_readerListener;
    eprosima::fastdds::dds::DataReader *m_dataReader;
    eprosima::fastdds::dds::Subscriber *m_subscriber;
};

template <typename T>
FastDDSTopicReader<T>::FastDDSTopicReader(
    eprosima::fastdds::dds::Subscriber *subscriber, eprosima::fastdds::dds::Topic *topic,
    OnMessageCallback callback, const eprosima::fastdds::dds::DataReaderQos &dataReaderQos,
    DataReaderListener<T> *listener)
    : m_subscriber(subscriber)
{
    if (nullptr != listener) {
        listener->setMessageCallback(std::move(callback));
        m_dataReader = subscriber->create_datareader(topic, dataReaderQos, listener);
    } else {
        m_readerListener.setMessageCallback(std::move(callback));
        m_dataReader = subscriber->create_datareader(topic, dataReaderQos, &m_readerListener);
    }
}

template <typename T>
FastDDSTopicReader<T>::~FastDDSTopicReader()
{
    if (m_dataReader && m_subscriber) {
        m_subscriber->delete_datareader(m_dataReader);
    }
}
} // namespace FastddsWrapper
#endif