/**
 * @file fastdds_qos_config.cc
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
#include "fastdds_qos_config.h"
#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.hpp>
#include <fastdds/rtps/transport/TCPv6TransportDescriptor.hpp>
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.hpp>
#include <fastdds/rtps/transport/UDPv6TransportDescriptor.hpp>
#include <fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.hpp>

namespace FastddsWrapper
{

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

ParticipantQoSBuilder::ParticipantQoSBuilder()
{
    qos_ = PARTICIPANT_QOS_DEFAULT;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::enableDiscovery(bool enable)
{
    // 可以在这里添加发现相关的配置
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setMaxMessageSize(uint32_t size)
{

    return *this;
}

const eprosima::fastdds::dds::DomainParticipantQos &ParticipantQoSBuilder::getQos() const
{
    return qos_;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setDiscoveryKeepAlive(uint32_t lease_duration_ms,
                                                                    uint32_t announcement_period_ms)
{
    qos_.wire_protocol().builtin.discovery_config.leaseDuration =
        eprosima::fastdds::dds::Duration_t(lease_duration_ms / 1000,
                                           (lease_duration_ms % 1000) * 1000000);
    qos_.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod =
        eprosima::fastdds::dds::Duration_t(announcement_period_ms / 1000,
                                           (announcement_period_ms % 1000) * 1000000);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::addUDPV4TransportInterfaces(
    const std::vector<std::string> &network_interfaces)
{
    auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
    qos_.transport().use_builtin_transports = false;
    if (!network_interfaces.empty()) {

        udp_transport->interfaceWhiteList.insert(udp_transport->interfaceWhiteList.end(),
                                                 network_interfaces.begin(),
                                                 network_interfaces.end());
    }
    udp_transport->interface_blocklist.push_back({"127.0.0.1"});
    qos_.transport().user_transports.push_back(udp_transport);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::addUDPV4TransportCoreId(int core_id)
{
    if (core_id < 0)
        return *this;
    eprosima::fastdds::rtps::ThreadSettings threadSetting;
    threadSetting.affinity = (1ULL << core_id);

    qos_.timed_events_thread(threadSetting);
    qos_.builtin_controllers_sender_thread(threadSetting);

    auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
    udp_transport->default_reception_threads(threadSetting);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setUserMulticastLocator(const std::string &address,
                                                                      uint16_t port)
{
    Locator locator;
    locator.kind = LOCATOR_KIND_UDPv4;
    IPLocator::setIPv4(locator, address);
    IPLocator::setPhysicalPort(locator, port);
    qos_.wire_protocol().default_multicast_locator_list.push_back(locator);
    return *this;
}

ParticipantQoSBuilder &
ParticipantQoSBuilder::setDiscoveryMulticastLocator(const std::string &address, uint16_t port)
{
    Locator locator;
    locator.kind = LOCATOR_KIND_UDPv4;
    IPLocator::setIPv4(locator, address);
    IPLocator::setPhysicalPort(locator, port);
    qos_.wire_protocol().builtin.metatrafficMulticastLocatorList.push_back(locator);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::addFlowController()
{
    // 设置流量控制器的参数
    // max_bytes_per_period: 定义在 period_ms 时间内，最多可以发送多少字节
    // period_ms: 定义一个周期的时间（毫秒）
    auto flow_controller = std::make_shared<eprosima::fastdds::rtps::FlowControllerDescriptor>();
    flow_controller->name = "reliable_flow_controller";
    flow_controller->scheduler = eprosima::fastdds::rtps::FlowControllerSchedulerPolicy::FIFO;
    // 1.2mbps | 150KBps, 系统默认udp缓存为213KB，留出余量给心跳及ACK，一发三收测试通过
    flow_controller->max_bytes_per_period = 1500;
    flow_controller->period_ms = 10;
    qos_.flow_controllers().push_back(flow_controller);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setParticipantQosProperties(const std::string &name,
                                                                          const std::string &value,
                                                                          bool propagate)
{
    qos_.properties().properties().emplace_back(name, value, propagate);
    return *this;
}

DataWriterQoSBuilder::DataWriterQoSBuilder()
{
    qos_ = DATAWRITER_QOS_DEFAULT;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setReliabilityKind(ReliabilityKind kind)
{
    using namespace eprosima::fastdds::dds;
    switch (kind) {
        case ReliabilityKind::BEST_EFFORT:
            qos_.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
            break;
        case ReliabilityKind::RELIABLE:
            qos_.reliability().kind = RELIABLE_RELIABILITY_QOS;
            break;
    }
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setReliabilityMaxBlockingTime(int milliseconds)
{
    qos_.reliability().max_blocking_time.seconds = milliseconds / 1000;
    qos_.reliability().max_blocking_time.nanosec = (milliseconds % 1000) * 1000000;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setDurabilityKind(DurabilityKind kind)
{
    using namespace eprosima::fastdds::dds;
    switch (kind) {
        case DurabilityKind::VOLATILE:
            qos_.durability().kind = VOLATILE_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT_LOCAL:
            qos_.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT:
            qos_.durability().kind = TRANSIENT_DURABILITY_QOS;
            break;
        case DurabilityKind::PERSISTENT:
            qos_.durability().kind = PERSISTENT_DURABILITY_QOS;
            break;
    }
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setHistoryKind(HistoryKind kind)
{
    using namespace eprosima::fastdds::dds;
    switch (kind) {
        case HistoryKind::KEEP_LAST:
            qos_.history().kind = KEEP_LAST_HISTORY_QOS;
            break;
        case HistoryKind::KEEP_ALL:
            qos_.history().kind = KEEP_ALL_HISTORY_QOS;
            break;
    }
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setHistoryDepth(int32_t depth)
{
    qos_.history().depth = depth;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setMaxSamples(int32_t max_samples)
{
    qos_.resource_limits().max_samples = max_samples;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setMaxInstances(int32_t max_instances)
{
    qos_.resource_limits().max_instances = max_instances;
    return *this;
}

DataWriterQoSBuilder &
DataWriterQoSBuilder::setMaxSamplesPerInstance(int32_t max_samples_per_instance)
{
    qos_.resource_limits().max_samples_per_instance = max_samples_per_instance;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::disableDataSharing()
{
    qos_.data_sharing().off();
    return *this;
}

const eprosima::fastdds::dds::DataWriterQos &DataWriterQoSBuilder::getQos() const
{
    return qos_;
}

DataReaderQoSBuilder::DataReaderQoSBuilder()
{
    qos_ = DATAREADER_QOS_DEFAULT;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setReliabilityKind(ReliabilityKind kind)
{
    using namespace eprosima::fastdds::dds;
    switch (kind) {
        case ReliabilityKind::BEST_EFFORT:
            qos_.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
            break;
        case ReliabilityKind::RELIABLE:
            qos_.reliability().kind = RELIABLE_RELIABILITY_QOS;
            break;
    }
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setDurabilityKind(DurabilityKind kind)
{
    using namespace eprosima::fastdds::dds;
    switch (kind) {
        case DurabilityKind::VOLATILE:
            qos_.durability().kind = VOLATILE_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT_LOCAL:
            qos_.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT:
            qos_.durability().kind = TRANSIENT_DURABILITY_QOS;
            break;
        case DurabilityKind::PERSISTENT:
            qos_.durability().kind = PERSISTENT_DURABILITY_QOS;
            break;
    }
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setHistoryKind(HistoryKind kind)
{
    using namespace eprosima::fastdds::dds;
    switch (kind) {
        case HistoryKind::KEEP_LAST:
            qos_.history().kind = KEEP_LAST_HISTORY_QOS;
            break;
        case HistoryKind::KEEP_ALL:
            qos_.history().kind = KEEP_ALL_HISTORY_QOS;
            break;
    }
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setHistoryDepth(int32_t depth)
{
    qos_.history().depth = depth;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setMaxSamples(int32_t max_samples)
{
    qos_.resource_limits().max_samples = max_samples;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setMaxInstances(int32_t max_instances)
{
    qos_.resource_limits().max_instances = max_instances;
    return *this;
}

DataReaderQoSBuilder &
DataReaderQoSBuilder::setMaxSamplesPerInstance(int32_t max_samples_per_instance)
{
    qos_.resource_limits().max_samples_per_instance = max_samples_per_instance;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::disableDataSharing()
{
    qos_.data_sharing().off();
    return *this;
}

const eprosima::fastdds::dds::DataReaderQos &DataReaderQoSBuilder::getQos() const
{
    return qos_;
}

namespace QoSPresets
{

    ParticipantQoSBuilder defaultParticipant()
    {
        return ParticipantQoSBuilder();
    }

    DataWriterQoSBuilder defaultWriter()
    {
        return DataWriterQoSBuilder();
    }

    DataReaderQoSBuilder defaultReader()
    {
        return DataReaderQoSBuilder();
    }

    DataWriterQoSBuilder reliableWriter()
    {
        return DataWriterQoSBuilder()
            .setDurabilityKind(DurabilityKind::TRANSIENT)
            .setReliabilityKind(ReliabilityKind::RELIABLE)
            .setHistoryKind(HistoryKind::KEEP_ALL);
    }

    DataReaderQoSBuilder reliableReader()
    {
        return DataReaderQoSBuilder()
            .setDurabilityKind(DurabilityKind::TRANSIENT)
            .setReliabilityKind(ReliabilityKind::RELIABLE)
            .setHistoryKind(HistoryKind::KEEP_ALL);
    }

} // namespace QoSPresets

} // namespace FastddsWrapper