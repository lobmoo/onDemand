/**
 * @file ParticipantQosHandler.cpp
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
#include "ParticipantQosHandler.h"

#include <fastdds/rtps/transport/TCPv4TransportDescriptor.hpp>
#include <fastdds/rtps/transport/TCPv6TransportDescriptor.hpp>
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.hpp>
#include <fastdds/rtps/transport/UDPv6TransportDescriptor.hpp>
#include <fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.hpp>

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

ParticipantQosHandler::ParticipantQosHandler(std::string participant_name)
{
    m_participantQos = PARTICIPANT_QOS_DEFAULT;
    m_participantQos.name(participant_name);
    // m_participantQos.properties().properties().emplace_back("fastdds.statistics",
    //     "HISTORY_LATENCY_TOPIC;ACKNACK_COUNT_TOPIC;DISCOVERY_TOPIC;PHYSICAL_DATA_TOPIC");
    // m_participantQos.wire_protocol().builtin.discovery_config.discoveryProtocol = DiscoveryProtocol::SIMPLE;
    // m_participantQos.wire_protocol().builtin.discovery_config.use_SIMPLE_EndpointDiscoveryProtocol = true;
    // m_participantQos.wire_protocol().builtin.discovery_config.m_simpleEDP.use_PublicationReaderANDSubscriptionWriter = true;
    // m_participantQos.wire_protocol().builtin.discovery_config.m_simpleEDP.use_PublicationWriterANDSubscriptionReader = true;
    // // 控制发现消息的广播频率
    eprosima::fastdds::dds::Time_t duration = eprosima::fastdds::dds::Duration_t(3, 0);
    m_participantQos.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod = duration;
    // 提示远端认为此 RTPSParticipant 应该存活的时间
    m_participantQos.wire_protocol().builtin.discovery_config.leaseDuration = eprosima::fastdds::dds::Duration_t(5, 0);
    // m_participantQos.name(participant_name);
    // // 设置为 false 可禁用默认的 UDPv4 传输方式, 默认为true
    // m_participantQos.transport().use_builtin_transports = false;
}

ParticipantQosHandler::~ParticipantQosHandler()
{
}

void ParticipantQosHandler::setParticipantQosProperties(std::string name, std::string value,
                                                        bool propagate)
{
    m_participantQos.properties().properties().emplace_back(name, value, propagate);
}

DomainParticipantExtendedQos &ParticipantQosHandler::getQos()
{
    return m_participantQos;
}

void ParticipantQosHandler::addSHMTransport(uint32_t segment_size)
{
    auto shm_transport = std::make_shared<SharedMemTransportDescriptor>();
    shm_transport->segment_size(segment_size);
    m_participantQos.transport().user_transports.push_back(shm_transport);
}

void ParticipantQosHandler::addTCPV4Transport(uint16_t listen_port,
                                              const std::vector<std::string> &peer_locators)
{
    auto tcp_transport = std::make_shared<TCPv4TransportDescriptor>();
    if (listen_port != 0) {
        tcp_transport->add_listener_port(listen_port);
    }
    for (int i = 0; i < peer_locators.size(); i++) {
        int position = peer_locators[i].find(':');
        if (position != peer_locators[i].npos) {
            std::string ipv4 = peer_locators[i].substr(0, position);
            std::string port = peer_locators[i].substr(position + 1);

            Locator_t initial_peer_locator;
            initial_peer_locator.kind = LOCATOR_KIND_TCPv4;
            IPLocator::setIPv4(initial_peer_locator, ipv4);
            initial_peer_locator.port = std::atoi(port.c_str());
            m_participantQos.wire_protocol().builtin.initialPeersList.push_back(
                initial_peer_locator);
        }
    }
    m_participantQos.transport().user_transports.push_back(tcp_transport);
}

void ParticipantQosHandler::addUDPV4Transport(uint32_t buffer_size,
                                              const std::vector<std::string> &ipaddrs)
{
    auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();

    udp_transport->sendBufferSize = buffer_size;
    udp_transport->receiveBufferSize = buffer_size;
    // 是否使用对 send_to() 的非阻塞调用
    udp_transport->non_blocking_send = true;
    for (int i = 0; i < ipaddrs.size(); i++) {
        Locator_t initial_peer_locator;
        IPLocator::setIPv4(initial_peer_locator, ipaddrs[i]);
        m_participantQos.wire_protocol().builtin.initialPeersList.push_back(initial_peer_locator);
    }
    m_participantQos.transport().user_transports.push_back(udp_transport);
}

void ParticipantQosHandler::addUDPV6Transport(uint32_t buffer_size)
{
    auto udp_transport = std::make_shared<UDPv6TransportDescriptor>();
    udp_transport->sendBufferSize = buffer_size;
    udp_transport->receiveBufferSize = buffer_size;
    udp_transport->non_blocking_send = true;
    m_participantQos.transport().user_transports.push_back(udp_transport);
}

void ParticipantQosHandler::add_statistics_and_monitor()
{
    m_participantQos.properties().properties().emplace_back("fastdds.enable_monitor_service",
                                                            "true");
    m_participantQos.properties().properties().emplace_back("fastdds.statistics",
                                                            "MONITOR_SERVICE_TOPIC;");

    m_participantQos.properties().properties().emplace_back("fastdds.statistics",
            "HISTORY_LATENCY_TOPIC;" \
            "NETWORK_LATENCY_TOPIC;" \
            "PUBLICATION_THROUGHPUT_TOPIC;" \
            "SUBSCRIPTION_THROUGHPUT_TOPIC;" \
            "RTPS_SENT_TOPIC;" \
            "RTPS_LOST_TOPIC;" \
            "HEARTBEAT_COUNT_TOPIC;" \
            "ACKNACK_COUNT_TOPIC;" \
            "NACKFRAG_COUNT_TOPIC;" \
            "GAP_COUNT_TOPIC;" \
            "DATA_COUNT_TOPIC;" \
            "RESENT_DATAS_TOPIC;" \
            "SAMPLE_DATAS_TOPIC;" \
            "PDP_PACKETS_TOPIC;" \
            "EDP_PACKETS_TOPIC;" \
            "DISCOVERY_TOPIC;" \
            "PHYSICAL_DATA_TOPIC;");    
}