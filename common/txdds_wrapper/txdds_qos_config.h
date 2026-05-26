/**
 * @file txdds_qos_config.h
 * @brief TXDDS QoS 封装（与 fastdds_wrapper 保持一致的接口形态）
 */
#ifndef TXDDS_QOS_CONFIG_H
#define TXDDS_QOS_CONFIG_H

#include <string>
#include <vector>

#include "txdds/DCPS/domain/qos/DomainParticipantQos.h"
#include "txdds/DCPS/publisher/qos/DataWriterQos.h"
#include "txdds/DCPS/publisher/qos/PublisherQos.h"
#include "txdds/DCPS/subscriber/qos/DataReaderQos.h"
#include "txdds/DCPS/subscriber/qos/SubscriberQos.h"
#include "txdds/RTPS/common/Locator.h"
#include "txdds/RTPS/common/Duration.h"
#include "txdds/RTPS/utils/IPLocator.h"
#include "txdds/RTPS/transport/ThreadConfig.h"

namespace TxddsWrapper
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
enum class HistoryKind { KEEP_LAST, KEEP_ALL };

class TXDDSNode;

class ParticipantQoSBuilder
{
public:
    ParticipantQoSBuilder();

    ParticipantQoSBuilder &enableDiscovery(bool enable);
    ParticipantQoSBuilder &setMaxMessageSize(uint32_t size);
    ParticipantQoSBuilder &setDiscoveryKeepAlive(uint32_t lease_duration_ms,
                                                 uint32_t announcement_period_ms);
    // Compatibility method with FastDDS wrapper call sites.
    ParticipantQoSBuilder &setInitialAnnouncements(uint32_t count, uint32_t period_ms);
    ParticipantQoSBuilder &
    addUDPV4TransportInterfaces(const std::vector<std::string> &network_interfaces = {}, uint32_t maxMessageSize = 0);
    ParticipantQoSBuilder &addUDPV4TransportCoreId(int core_id);
    ParticipantQoSBuilder &setUserMulticastLocator(const std::string &address, uint16_t port);
    ParticipantQoSBuilder &setUserUnicastLocator(const std::string &address, uint16_t port);
    ParticipantQoSBuilder &setDiscoveryMulticastLocator(const std::string &address, uint16_t port);
    ParticipantQoSBuilder &addFlowController();
    ParticipantQoSBuilder &setIgnoreLocalEndpoints();
    ParticipantQoSBuilder &setParticipantQosProperties(const std::string &name,
                                                       const std::string &value, bool propagate);

private:
    const BaoSky::dds::DomainParticipantQos &getQos() const;
    friend class TXDDSNode;

    BaoSky::dds::DomainParticipantQos qos_;
};

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
    const BaoSky::dds::DataWriterQos &getQos() const;
    friend class TXDDSNode;

    BaoSky::dds::DataWriterQos qos_;
};

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
    const BaoSky::dds::DataReaderQos &getQos() const;
    friend class TXDDSNode;

    BaoSky::dds::DataReaderQos qos_;
};

/**
 * @brief Publisher QoS 构造器（兼容 fastdds_wrapper）
 */
class PublisherQoSBuilder
{
public:
    PublisherQoSBuilder();

    PublisherQoSBuilder &setPartition(const std::string &partition);
    PublisherQoSBuilder &setAutoEnable(bool enable);

private:
    const BaoSky::dds::PublisherQos &getQos() const;
    friend class TXDDSNode;

    BaoSky::dds::PublisherQos qos_;
};

/**
 * @brief Subscriber QoS 构造器（兼容 fastdds_wrapper）
 */
class SubscriberQoSBuilder
{
public:
    SubscriberQoSBuilder();

    SubscriberQoSBuilder &setPartition(const std::string &partition);
    SubscriberQoSBuilder &setAutoEnable(bool enable);

private:
    const BaoSky::dds::SubscriberQos &getQos() const;
    friend class TXDDSNode;

    BaoSky::dds::SubscriberQos qos_;
};

namespace QoSPresets
{
    ParticipantQoSBuilder defaultParticipant();

    DataWriterQoSBuilder defaultWriter();
    DataWriterQoSBuilder reliableWriter();

    DataReaderQoSBuilder defaultReader();
    DataReaderQoSBuilder reliableReader();

    PublisherQoSBuilder defaultPublisher();
    SubscriberQoSBuilder defaultSubscriber();
} // namespace QoSPresets

} // namespace TxddsWrapper

#endif // TXDDS_QOS_CONFIG_H
