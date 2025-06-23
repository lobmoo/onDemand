/**
 * @file ParticipantQosHandler.h
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
#ifndef PARTICIPANT_QOS_HANDLER_H
#define PARTICIPANT_QOS_HANDLER_H

#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantExtendedQos.hpp>

class ParticipantQosHandler
{
public:
    ParticipantQosHandler(std::string participant_name);
    ~ParticipantQosHandler();

public:
    eprosima::fastdds::dds::DomainParticipantExtendedQos &getQos();

public:
    /**
     * @brief 使用共享内存
     *
     * @param segment_size
     */
    void addSHMTransport(uint32_t segment_size);

    /**
     * @brief 使用TCP v4方式传输
     *
     * @param listen_port
     * @param peer_locators IP端口列表(格式: 127.0.0.1:10001)
     */
    void addTCPV4Transport(uint16_t listen_port, const std::vector<std::string> &peer_locators);

    /**
     * @brief 添加 TCPv6 策略
     *
     */
    void addTCPV6Transport();

    /**
     * @brief 添加 UDPv4 策略
     *
     * @param buffer_size
     * @param ipaddrs IP列表
     */
    void addUDPV4Transport(uint32_t buffer_size = 1024 * 1024 * 16,
                           const std::vector<std::string> &ipaddrs = {});

    /**
     * @brief 添加 UDPv6 策略
     *
     * @param buffer_size
     */
    void addUDPV6Transport(uint32_t buffer_size = 1024 * 1024 * 16);

    /**
     * @brief Set the Participant Qos Properties object
     * @param  name             
     * @param  value            
     * @param  propagate        
     */
    void setParticipantQosProperties(std::string name, std::string value, bool propagate);

    /**
     * @brief open statistics and monitor
     */
    void add_statistics_and_monitor();

    /**
     * @brief 设置网络接口
     * @param  network_interfaceMyParamDoc
     */
    void addUDPV4TransportInterface(std::string network_interface);

    /**
     * @brief Set the Discovery Keep Alive object
     * @param  lease_duration_ms 提示远端认为此 RTPSParticipant 应该存活的时间,如果其他参与者在这一时间内没有收到该参与者的保活消息，则认为对端挂掉
     * @param  announcement_period_ms 对外保活公告周期，即每隔多少毫秒发送一次保活消息  这个参数一般要小于 lease_duration_ms的1/3
     * @return * void 
     */
    void setDiscoveryKeepAlive(uint32_t lease_duration_ms, uint32_t announcement_period_ms);
private:
    //eprosima::fastdds::dds::DomainParticipantQos m_participantQos;
    eprosima::fastdds::dds::DomainParticipantExtendedQos m_participantQos;
};

#endif