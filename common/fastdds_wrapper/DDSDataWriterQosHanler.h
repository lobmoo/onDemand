/**
 * @file DDSDataWriterQosHanler.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-06-23
 * 
 * @copyright Copyright (c) 2025  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-06-23     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef DDS_DATA_WRITER_QOS_HANDLER_H
#define DDS_DATA_WRITER_QOS_HANDLER_H

#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

class DDSDataWriterQosHanler
{

public:
    DDSDataWriterQosHanler(/* args */);
    ~DDSDataWriterQosHanler();
    const eprosima::fastdds::dds::DataWriterQos &getQos() const;

    void setDurability(u_int32_t depth);
    void setResourceLimits(uint32_t max_samples, uint32_t max_instances,
                           uint32_t max_samples_per_instance);

private:
    eprosima::fastdds::dds::DataWriterQos m_dataWriterQos;
};

#endif // DDS_DATA_WRITER_QOS_HANDLER_H