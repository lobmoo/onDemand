/**
 * @file fastdds_qos_config.h
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
#ifndef FASTDDS_QOS_CONFIG_H
#define FASTDDS_QOS_CONFIG_H

#include <string>
#include <vector>

// 包含完整的类型定义（成员变量需要完整类型）
// 但公共接口不暴露这些类型给用户
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>

namespace FastddsWrapper
{

/**
 * @brief 可靠性 QoS 策略类型
 */
enum class ReliabilityKind { BEST_EFFORT, RELIABLE };

/**
 * @brief 持久性 QoS 策略类型
 */
enum class DurabilityKind { VOLATILE, TRANSIENT_LOCAL, TRANSIENT, PERSISTENT };

/**
 * @brief 历史 QoS 策略类型
 */
enum class HistoryKind {
    KEEP_LAST, // 保留最后 N 个样本
    KEEP_ALL   // 保留所有样本
};

// 前向声明
class FastDataNode;

/**
 * @brief ParticipantQoSBuilder
 */
class ParticipantQoSBuilder
{
public:
    ParticipantQoSBuilder();

    ParticipantQoSBuilder &setName(const std::string &name);
    ParticipantQoSBuilder &enableDiscovery(bool enable);


    // 扩展方法以支持更多配置
    ParticipantQoSBuilder &setDiscoveryKeepAlive(uint32_t lease_duration_ms,
                                                 uint32_t announcement_period_ms);
    ParticipantQoSBuilder &setInitialAnnouncements(uint32_t count, uint32_t period_ms);
    ParticipantQoSBuilder &
    addUDPV4TransportInterfaces(const std::vector<std::string> &network_interfaces ,uint32_t maxMessageSize = 0);
    ParticipantQoSBuilder &addUDPV4TransportCoreId(int core_id);
    ParticipantQoSBuilder &setUserMulticastLocator(const std::string &address, uint16_t port);
    ParticipantQoSBuilder &setUserUnicastLocator(const std::string &address, uint16_t port);
    ParticipantQoSBuilder &setDiscoveryMulticastLocator(const std::string &address, uint16_t port);
    ParticipantQoSBuilder &addFlowController();
    ParticipantQoSBuilder &setIgnoreLocalEndpoints();
    ParticipantQoSBuilder &setParticipantQosProperties(const std::string &name,
                                                       const std::string &value, bool propagate);
    ParticipantQoSBuilder &setupLargeDataTransport();

private:
    // 内部方法：获取原生 QoS（仅内部使用，友元访问）
    const eprosima::fastdds::dds::DomainParticipantQos &getQos() const;

    friend class FastDataNode;

    eprosima::fastdds::dds::DomainParticipantQos qos_;
};

/**
 * @brief DataWriterQoSBuilder
 */
class DataWriterQoSBuilder
{
public:
    DataWriterQoSBuilder();

    DataWriterQoSBuilder &setReliabilityKind(ReliabilityKind kind);
    DataWriterQoSBuilder &setReliabilityMaxBlockingTime(int milliseconds);
    DataWriterQoSBuilder &setDurabilityKind(DurabilityKind kind);
    DataWriterQoSBuilder &setHistoryKind(HistoryKind kind);
    DataWriterQoSBuilder &setHistoryDepth(int32_t depth);
    DataWriterQoSBuilder &setMaxSamples(int32_t max_samples);
    DataWriterQoSBuilder &setMaxInstances(int32_t max_instances);
    DataWriterQoSBuilder &setMaxSamplesPerInstance(int32_t max_samples_per_instance);
    DataWriterQoSBuilder &disableDataSharing();
    DataWriterQoSBuilder &writer_resource_limits(int32_t max_matched_readers);
    DataWriterQoSBuilder &setFlowController(const std::string &flow_controller_name);
    DataWriterQoSBuilder &setAsyncPublisherMode(bool async);

private:
    // 内部方法：获取原生 QoS（仅内部使用，友元访问）
    const eprosima::fastdds::dds::DataWriterQos &getQos() const;

    friend class FastDataNode;

    eprosima::fastdds::dds::DataWriterQos qos_;
};

/**
 * @brief DataReaderQoSBuilder
 */
class DataReaderQoSBuilder
{
public:
    DataReaderQoSBuilder();

    DataReaderQoSBuilder &setReliabilityKind(ReliabilityKind kind);
    DataReaderQoSBuilder &setDurabilityKind(DurabilityKind kind);
    DataReaderQoSBuilder &setHistoryKind(HistoryKind kind);
    DataReaderQoSBuilder &setHistoryDepth(int32_t depth);
    DataReaderQoSBuilder &setMaxSamples(int32_t max_samples);
    DataReaderQoSBuilder &setMaxInstances(int32_t max_instances);
    DataReaderQoSBuilder &setMaxSamplesPerInstance(int32_t max_samples_per_instance);
    DataReaderQoSBuilder &disableDataSharing();
    DataReaderQoSBuilder &reader_resource_limits(int32_t max_matched_writer);

private:
    // 内部方法：获取原生 QoS（仅内部使用，友元访问）
    const eprosima::fastdds::dds::DataReaderQos &getQos() const;

    friend class FastDataNode;

    eprosima::fastdds::dds::DataReaderQos qos_;
};

/**
 * @brief PublisherQoSBuilder — 控制 Publisher 实体级 QoS（partition、entity_factory）
 */
class PublisherQoSBuilder
{
public:
    PublisherQoSBuilder();

    /// 设置 partition 名称（空字符串表示默认 partition）
    PublisherQoSBuilder &setPartition(const std::string &partition);
    /// 是否自动 enable 新创建的 DataWriter（默认 true）
    PublisherQoSBuilder &setAutoEnable(bool enable);

private:
    const eprosima::fastdds::dds::PublisherQos &getQos() const;

    friend class FastDataNode;

    eprosima::fastdds::dds::PublisherQos qos_;
};

/**
 * @brief SubscriberQoSBuilder — 控制 Subscriber 实体级 QoS（partition、entity_factory）
 */
class SubscriberQoSBuilder
{
public:
    SubscriberQoSBuilder();

    /// 设置 partition 名称（空字符串表示默认 partition）
    SubscriberQoSBuilder &setPartition(const std::string &partition);
    /// 是否自动 enable 新创建的 DataReader（默认 true）
    SubscriberQoSBuilder &setAutoEnable(bool enable);

private:
    const eprosima::fastdds::dds::SubscriberQos &getQos() const;

    friend class FastDataNode;

    eprosima::fastdds::dds::SubscriberQos qos_;
};

/**
 * @brief 节点级 QoS 聚合配置，传给 FastDataNode 构造函数
 */
struct NodeQoSConfig {
    ParticipantQoSBuilder participant;
    PublisherQoSBuilder   publisher;
    SubscriberQoSBuilder  subscriber;
};

/**
 * @brief QoS预设配置
 */
namespace QoSPresets
{
    // Participant预设
    ParticipantQoSBuilder defaultParticipant();

    // Writer预设
    DataWriterQoSBuilder defaultWriter();
    DataWriterQoSBuilder reliableWriter();

    // Reader预设
    DataReaderQoSBuilder defaultReader();
    DataReaderQoSBuilder reliableReader();

    // Publisher/Subscriber预设
    PublisherQoSBuilder defaultPublisher();
    SubscriberQoSBuilder defaultSubscriber();

} // namespace QoSPresets

} // namespace FastddsWrapper

#endif // FASTDDS_QOS_CONFIG_H