#include "txdds_qos_config.h"
#include "txdds/RTPS/transport/ThreadManager.h"
#include "txdds/RTPS/transport/TransportStack.h"
#include "txdds/RTPS/transport/TransportConfig.h"

namespace TxddsWrapper
{

std::atomic<uint32_t> ParticipantQoSBuilder::instance_counter_{0};

namespace
{
    BaoSky::rtps::Duration toDuration(uint32_t milliseconds)
    {
        return BaoSky::rtps::Duration(static_cast<int32_t>(milliseconds / 1000),
                                      static_cast<uint32_t>((milliseconds % 1000) * 1000000ULL));
    }

    BaoSky::rtps::Locator makeIPv4Locator(const std::string &address, uint32_t port,
                                           const std::string &configName)
    {
        BaoSky::rtps::Locator locator;
        locator.mKind = BaoSky::rtps::LOCATOR_KIND_UDPv4;
        locator.mPort = port;
        locator.mConfigName = configName;
        BaoSky::rtps::IPLocator::setIPv4(locator, address);
        return locator;
    }
} // namespace

ParticipantQoSBuilder::ParticipantQoSBuilder()
{
    const uint32_t id = instance_counter_.fetch_add(1, std::memory_order_relaxed);
    threadCfgName_ = "dsf_connector_" + std::to_string(id);
    transportCfgName_ = "udp_" + std::to_string(id);

    BaoSky::rtps::ThreadConfig thread_config;
    thread_config.mConfigName = threadCfgName_;
    thread_config.mThreadName = threadCfgName_;
    BaoSky::rtps::ThreadManager::GetInstance()->AddResource(thread_config);

    auto udp_config = std::make_shared<BaoSky::rtps::UDPTransportConfig>();
    udp_config->mKind = BaoSky::rtps::eTransportKind::UDPTransportKind;
    udp_config->mLocalIP.push_back("0.0.0.0");
    udp_config->mConfigName = transportCfgName_;
    udp_config->mThreadConfigName = threadCfgName_;
    udp_config->mRecvBufferSize = 16 * 1024 * 1024;
    udp_config->mSendBufferSize = 16 * 1024 * 1024;
    BaoSky::rtps::TransportStack::GetInstance()->AddConfig(udp_config);

    auto locatorUser = makeIPv4Locator("239.255.0.1", 7400, transportCfgName_);
    auto locatorBuiltin = makeIPv4Locator("239.255.0.1", 7400, transportCfgName_);
    qos_.wireProtocol.builtin.mDiscoveryAttributes.SetEnableInternalEntityMatch(true);
    qos_.wireProtocol.defaultMulticastLocatorList.push_back(locatorUser);
    qos_.wireProtocol.builtin.multicastLocatorList.push_back(locatorBuiltin); //g公告
}

ParticipantQoSBuilder &ParticipantQoSBuilder::enableDiscovery(bool enable)
{
    qos_.wireProtocol.builtin.mDiscoveryAttributes.mPDPMode =
        enable ? BaoSky::rtps::PDPMode::SIMPLE : BaoSky::rtps::PDPMode::NONE;
    qos_.wireProtocol.builtin.mDiscoveryAttributes.mIsEnableDiscovery = enable;
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setMaxMessageSize(uint32_t size)
{
    qos_.wireProtocol.builtin.mBuiltinReaderPayloadSize = size;
    qos_.wireProtocol.builtin.mBuiltinWriterPayloadSize = size;
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setDiscoveryKeepAlive(uint32_t lease_duration_ms,
                                                                    uint32_t announcement_period_ms)
{
    qos_.wireProtocol.builtin.mDiscoveryAttributes.mLeaseDuration = toDuration(lease_duration_ms);
    qos_.wireProtocol.builtin.mDiscoveryAttributes.mAnnouncePeriod =
        toDuration(announcement_period_ms);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setInitialAnnouncements(uint32_t count,
                                                                      uint32_t period_ms)
{
    // TXDDS does not expose FastDDS initial_announcements directly.
    // Approximate behavior by keeping the same announcement period and scaling lease duration.
    // const uint32_t lease_ms = (count == 0) ? period_ms : count * period_ms;
    // return setDiscoveryKeepAlive(lease_ms, period_ms);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::addUDPV4TransportInterfaces(
    const std::vector<std::string> &network_interfaces,  uint32_t maxMessageSize)
{
    BaoSky::rtps::TransportStack::GetInstance()->DeleteConfig(transportCfgName_);
    auto udp_config = std::make_shared<BaoSky::rtps::UDPTransportConfig>();
    udp_config->mKind = BaoSky::rtps::eTransportKind::UDPTransportKind;
    udp_config->mConfigName = transportCfgName_;
    udp_config->mThreadConfigName = threadCfgName_;
    udp_config->mRecvBufferSize = 16 * 1024 * 1024;
    udp_config->mSendBufferSize = 16 * 1024 * 1024;
    if (network_interfaces.empty()) {
        udp_config->mLocalIP.push_back("0.0.0.0");
    } else {
        udp_config->mLocalIP = network_interfaces;
    }
    if (maxMessageSize > 0) {
        //udp_config->mMaxMessageSize = maxMessageSize;
    }
    BaoSky::rtps::TransportStack::GetInstance()->AddConfig(udp_config);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setIgnoreLocalEndpoints()
{
    //qos_.properties().properties().emplace_back("fastdds.ignore_local_endpoints", "true");
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setUserMulticastLocator(const std::string &ip,
                                                                      uint16_t port)
{
    BaoSky::rtps::Locator locator;
    locator.mConfigName = transportCfgName_;
    BaoSky::rtps::IPLocator::setIPv4(locator, ip);
    BaoSky::rtps::IPLocator::setPhysicalPort(locator, port);
    qos_.wireProtocol.defaultMulticastLocatorList.Clear();
    qos_.wireProtocol.defaultMulticastLocatorList.push_back(locator);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setUserUnicastLocator(const std::string &ip,
                                                                    uint16_t port)
{
    BaoSky::rtps::Locator locator;
    locator.mConfigName = transportCfgName_;
    BaoSky::rtps::IPLocator::setIPv4(locator, ip);
    BaoSky::rtps::IPLocator::setPhysicalPort(locator, port);
    qos_.wireProtocol.defaultUnicastLocatorList.push_back(locator);
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setDiscoveryMulticastLocator(const std::string &ip,
                                                                           uint16_t port)
{
    BaoSky::rtps::Locator locator;
    locator.mConfigName = transportCfgName_;
    BaoSky::rtps::IPLocator::setIPv4(locator, ip);
    BaoSky::rtps::IPLocator::setPhysicalPort(locator, port);
    qos_.wireProtocol.builtin.multicastLocatorList.Clear();
    qos_.wireProtocol.builtin.multicastLocatorList.push_back(locator); //公告
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::addUDPV4TransportCoreId(int core_id)
{
    if (core_id >= 0) {
        qos_.mEventThreadConfig.mCPUSet.clear();
        qos_.mEventThreadConfig.mCPUSet.push_back(static_cast<uint16_t>(core_id));
    }
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::addFlowController()
{
    return *this;
}

ParticipantQoSBuilder &ParticipantQoSBuilder::setParticipantQosProperties(const std::string &,
                                                                          const std::string &, bool)
{
    return *this;
}

const BaoSky::dds::DomainParticipantQos &ParticipantQoSBuilder::getQos() const
{
    return qos_;
}

DataWriterQoSBuilder::DataWriterQoSBuilder()
{
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setReliabilityKind(ReliabilityKind kind)
{
    qos_.reliability.mKind =
        (kind == ReliabilityKind::RELIABLE)
            ? BaoSky::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS
            : BaoSky::dds::ReliabilityQosPolicyKind::BEST_EFFORT_RELIABILITY_QOS;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setReliabilityMaxBlockingTime(int milliseconds)
{
    qos_.reliability.mMaxBlockingTime = toDuration(milliseconds);
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setDurabilityKind(DurabilityKind kind)
{
    using Kind = BaoSky::dds::DurabilityQosPolicyKind;
    switch (kind) {
        case DurabilityKind::VOLATILE:
            qos_.durability.mKind = Kind::VOLATILE_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT_LOCAL:
            qos_.durability.mKind = Kind::TRANSIENT_LOCAL_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT:
            qos_.durability.mKind = Kind::TRANSIENT_DURABILITY_QOS;
            break;
        case DurabilityKind::PERSISTENT:
            qos_.durability.mKind = Kind::PERSISTENT_DURABILITY_QOS;
            break;
    }
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setHistoryKind(HistoryKind kind)
{
    qos_.history.mKind = (kind == HistoryKind::KEEP_ALL)
                             ? BaoSky::dds::HistoryQosPolicyKind::KEEP_ALL_HISTORY_QOS
                             : BaoSky::dds::HistoryQosPolicyKind::KEEP_LAST_HISTORY_QOS;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setHistoryDepth(int32_t depth)
{
    qos_.history.mDepth = depth;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setMaxSamples(int32_t max_samples)
{
    qos_.resource_limits.mMaxSamples = max_samples;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setMaxInstances(int32_t max_instances)
{
    qos_.resource_limits.mMaxInstance = max_instances;
    return *this;
}

DataWriterQoSBuilder &
DataWriterQoSBuilder::setMaxSamplesPerInstance(int32_t max_samples_per_instance)
{
    qos_.resource_limits.mMaxSamplesPerInstance = max_samples_per_instance;
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::writer_resource_limits(int32_t max_matched_readers)
{
    // qos_.writer_resource_limits().matched_subscriber_allocation =
    //     eprosima::fastdds::ResourceLimitedContainerConfig(0, max_matched_readers, 1u);
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::disableDataSharing()
{
    return *this;
}

DataWriterQoSBuilder &DataWriterQoSBuilder::setAsyncPublisherMode(bool async)
{
    //qos_.publish_mode().kind = async ? ASYNCHRONOUS_PUBLISH_MODE : SYNCHRONOUS_PUBLISH_MODE;
    return *this;
}

DataWriterQoSBuilder &
DataWriterQoSBuilder::setFlowController(const std::string &flow_controller_name)
{
    // qos_.publish_mode().kind = ASYNCHRONOUS_PUBLISH_MODE;
    // qos_.publish_mode().flow_controller_name = flow_controller_name;
    return *this;
}

const BaoSky::dds::DataWriterQos &DataWriterQoSBuilder::getQos() const
{
    return qos_;
}

DataReaderQoSBuilder::DataReaderQoSBuilder()
{
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setReliabilityKind(ReliabilityKind kind)
{
    qos_.reliability.mKind =
        (kind == ReliabilityKind::RELIABLE)
            ? BaoSky::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS
            : BaoSky::dds::ReliabilityQosPolicyKind::BEST_EFFORT_RELIABILITY_QOS;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setDurabilityKind(DurabilityKind kind)
{
    using Kind = BaoSky::dds::DurabilityQosPolicyKind;
    switch (kind) {
        case DurabilityKind::VOLATILE:
            qos_.durability.mKind = Kind::VOLATILE_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT_LOCAL:
            qos_.durability.mKind = Kind::TRANSIENT_LOCAL_DURABILITY_QOS;
            break;
        case DurabilityKind::TRANSIENT:
            qos_.durability.mKind = Kind::TRANSIENT_DURABILITY_QOS;
            break;
        case DurabilityKind::PERSISTENT:
            qos_.durability.mKind = Kind::PERSISTENT_DURABILITY_QOS;
            break;
    }
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setHistoryKind(HistoryKind kind)
{
    qos_.history.mKind = (kind == HistoryKind::KEEP_ALL)
                             ? BaoSky::dds::HistoryQosPolicyKind::KEEP_ALL_HISTORY_QOS
                             : BaoSky::dds::HistoryQosPolicyKind::KEEP_LAST_HISTORY_QOS;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setHistoryDepth(int32_t depth)
{
    qos_.history.mDepth = depth;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setMaxSamples(int32_t max_samples)
{
    qos_.resource_limits.mMaxSamples = max_samples;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::setMaxInstances(int32_t max_instances)
{
    qos_.resource_limits.mMaxInstance = max_instances;
    return *this;
}

DataReaderQoSBuilder &
DataReaderQoSBuilder::setMaxSamplesPerInstance(int32_t max_samples_per_instance)
{
    qos_.resource_limits.mMaxSamplesPerInstance = max_samples_per_instance;
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::disableDataSharing()
{
    return *this;
}

DataReaderQoSBuilder &DataReaderQoSBuilder::reader_resource_limits(int32_t max_matched_writer)
{
    // qos_.reader_resource_limits().matched_publisher_allocation =
    //     eprosima::fastdds::ResourceLimitedContainerConfig(0, max_matched_writer, 1u);
    return *this;
}

const BaoSky::dds::DataReaderQos &DataReaderQoSBuilder::getQos() const
{
    return qos_;
}

PublisherQoSBuilder::PublisherQoSBuilder()
{
}

PublisherQoSBuilder &PublisherQoSBuilder::setPartition(const std::string &partition)
{
    if (!partition.empty()) {
        qos_.partition.mName.push_back(partition);
    }
    return *this;
}

PublisherQoSBuilder &PublisherQoSBuilder::setAutoEnable(bool enable)
{
    qos_.entity_factory.mAutoEnableCreatedEntities = enable;
    return *this;
}

const BaoSky::dds::PublisherQos &PublisherQoSBuilder::getQos() const
{
    return qos_;
}

SubscriberQoSBuilder::SubscriberQoSBuilder()
{
}

SubscriberQoSBuilder &SubscriberQoSBuilder::setPartition(const std::string &partition)
{
    if (!partition.empty()) {
        qos_.partition.mName.push_back(partition);
    }
    return *this;
}

SubscriberQoSBuilder &SubscriberQoSBuilder::setAutoEnable(bool /*enable*/)
{
    // TXDDS SubscriberQos currently has no entity_factory equivalent; keep API compatibility.
    return *this;
}

const BaoSky::dds::SubscriberQos &SubscriberQoSBuilder::getQos() const
{
    return qos_;
}

namespace QoSPresets
{
    ParticipantQoSBuilder defaultParticipant()
    {
        ParticipantQoSBuilder builder;
        return builder;
    }

    DataWriterQoSBuilder defaultWriter()
    {
        DataWriterQoSBuilder builder;
        return builder;
    }

    DataWriterQoSBuilder reliableWriter()
    {
        return DataWriterQoSBuilder()
            .setReliabilityKind(ReliabilityKind::RELIABLE)
            .setDurabilityKind(DurabilityKind::TRANSIENT_LOCAL)
            .setHistoryKind(HistoryKind::KEEP_ALL);
    }

    DataReaderQoSBuilder defaultReader()
    {
        DataReaderQoSBuilder builder;
        return builder;
    }

    DataReaderQoSBuilder reliableReader()
    {
        return DataReaderQoSBuilder()
            .setReliabilityKind(ReliabilityKind::RELIABLE)
            .setDurabilityKind(DurabilityKind::TRANSIENT_LOCAL)
            .setHistoryKind(HistoryKind::KEEP_ALL);
    }

    PublisherQoSBuilder defaultPublisher()
    {
        PublisherQoSBuilder builder;
        return builder;
    }

    SubscriberQoSBuilder defaultSubscriber()
    {
        SubscriberQoSBuilder builder;
        return builder;
    }
} // namespace QoSPresets

} // namespace TxddsWrapper