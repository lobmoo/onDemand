#include "DDSDataWriterQosHanler.h"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

DDSDataWriterQosHanler::DDSDataWriterQosHanler(/* args */)
{
    m_dataWriterQos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
}

DDSDataWriterQosHanler::~DDSDataWriterQosHanler()
{

}

const DataWriterQos& DDSDataWriterQosHanler::getQos() const
{
    return m_dataWriterQos;
}

/**
 * @brief 配置写入器的消息持久化
 * @param  depth  需要历史保留的消息数
 */
void DDSDataWriterQosHanler::setDurability(u_int32_t depth)
{
    m_dataWriterQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    m_dataWriterQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
    m_dataWriterQos.history().kind = KEEP_LAST_HISTORY_QOS;
    m_dataWriterQos.history().depth = static_cast<int32_t>(depth);
}


/**
 * @brief  配置dds写入器的资源限制
 * @param  max_samples  所有实例加起来最大能存多少样本总数     
 * @param  max_instances   DataWriter 或 DataReader 能管理的最大实例
 * @param  max_samples_per_instanceMyParamDoc 单个实例最多能存多少样本
 */
void DDSDataWriterQosHanler::setResourceLimits(uint32_t max_samples = 0, uint32_t max_instances = 0,
                                               uint32_t max_samples_per_instance = 0)
{
    m_dataWriterQos.resource_limits().max_samples = static_cast<int32_t>(max_samples);
    m_dataWriterQos.resource_limits().max_instances = static_cast<int32_t>(max_instances);
    m_dataWriterQos.resource_limits().max_samples_per_instance =
        static_cast<int32_t>(max_samples_per_instance);
}

