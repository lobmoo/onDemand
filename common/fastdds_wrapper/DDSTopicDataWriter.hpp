/**
 * @file DDSTopicDataWriter.hpp
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
#ifndef DDS_TOPIC_DATAWRITER_H
#define DDS_TOPIC_DATAWRITER_H

#include "DDSDataWriterListener.hpp"
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

template <typename T>
class DDSTopicDataWriter
{
public:
    DDSTopicDataWriter(eprosima::fastdds::dds::Publisher *publisher,
                       eprosima::fastdds::dds::Topic *topic,
                       const eprosima::fastdds::dds::DataWriterQos &dataWriterQos);
    ~DDSTopicDataWriter();

public:
    bool writeMessage(const T &message);
    bool clear_history(size_t *removed);

private:
    DDSDataWriterListener m_writerListener;
    eprosima::fastdds::dds::DataWriter *m_dataWriter;
    eprosima::fastdds::dds::Publisher *m_publisher;
};

template <typename T>
DDSTopicDataWriter<T>::DDSTopicDataWriter(
    eprosima::fastdds::dds::Publisher *publisher, eprosima::fastdds::dds::Topic *topic,
    const eprosima::fastdds::dds::DataWriterQos &dataWriterQos)
    : m_publisher(publisher)
{
    m_dataWriter = publisher->create_datawriter(topic, dataWriterQos, &m_writerListener);
}

template <typename T>
DDSTopicDataWriter<T>::~DDSTopicDataWriter()
{
    if (m_dataWriter) {
        m_publisher->delete_datawriter(m_dataWriter);
    }
}

template <typename T>
bool DDSTopicDataWriter<T>::writeMessage(const T &message)
{
    return m_dataWriter->write((void *)&message) == eprosima::fastdds::dds::RETCODE_OK ? true
                                                                                       : false;
}

template <typename T>
bool DDSTopicDataWriter<T>::clear_history(size_t *removed)
{
    return m_dataWriter->clear_history(removed) == eprosima::fastdds::dds::RETCODE_OK ? true
                                                                                      : false;
}
#endif