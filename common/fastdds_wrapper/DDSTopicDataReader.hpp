/**
 * @file DDSTopicDataReader.hpp
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
#ifndef DDS_TOPIC_DATAREADER_H
#define DDS_TOPIC_DATAREADER_H

#include "DDSDataReaderListener.hpp"
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>

template <typename T>
class DDSTopicDataReader
{
    typedef std::function<void(const std::string &, std::shared_ptr<T>)> OnMessageCallback;

public:
    DDSTopicDataReader(eprosima::fastdds::dds::Subscriber          *subscriber,
                       eprosima::fastdds::dds::Topic               *topic,
                       OnMessageCallback                            callback,
                       const eprosima::fastdds::dds::DataReaderQos &dataReaderQos);
    ~DDSTopicDataReader();

private:
    DDSDataReaderListener<T>            m_readerListener;
    eprosima::fastdds::dds::DataReader *m_dataReader;
    eprosima::fastdds::dds::Subscriber *m_subscriber;
};

template <typename T>
DDSTopicDataReader<T>::DDSTopicDataReader(eprosima::fastdds::dds::Subscriber          *subscriber,
                                          eprosima::fastdds::dds::Topic               *topic,
                                          OnMessageCallback                            callback,
                                          const eprosima::fastdds::dds::DataReaderQos &dataReaderQos) :
    m_subscriber(subscriber)
{
    m_readerListener.m_callback = callback;
    m_dataReader = subscriber->create_datareader(topic, dataReaderQos, &m_readerListener);
}

template <typename T>
DDSTopicDataReader<T>::~DDSTopicDataReader()
{
    if (m_dataReader)
        m_subscriber->delete_datareader(m_dataReader);
}

#endif