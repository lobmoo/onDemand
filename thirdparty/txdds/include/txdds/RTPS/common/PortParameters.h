/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-07-01 13:38:51
 * @FilePath     : /TXDDS/include/txdds/RTPS/common/PortParameters.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_PORTPARAMETERS_H
#define TXDDS_RTPS_PORTPARAMETERS_H

#include <cstdint>

namespace BaoSky::rtps
{
    class PortParameters
    {
    public:
        PortParameters() : mPortBase(7400), mDomainIDGain(250), mParticipantIDGain(2), mOffsetd0(0), mOffsetd1(10), mOffsetd2(1), mOffsetd3(11)
        {
        }

        virtual ~PortParameters()
        {
        }

        inline uint16_t CalculateDiscoveryPort(uint32_t domainId, uint32_t RTPSParticipantID = 0, bool isMulticast = true) const
        {
            uint32_t port = mPortBase + mDomainIDGain * domainId + (isMulticast ? mOffsetd0 : (mOffsetd1 + mParticipantIDGain * RTPSParticipantID));
            port = port > 65535 ? 0 : port;
            return static_cast<uint16_t>(port);
        }

        inline uint16_t CalculateWellKnownPort(uint32_t domain_id, uint32_t RTPSParticipantID = 0, bool isMulticast = true) const
        {
            uint32_t port = mPortBase + mDomainIDGain * domain_id + (isMulticast ? mOffsetd2 : (mOffsetd3 + mParticipantIDGain * RTPSParticipantID));
            port = port > 65535 ? 0 : port;
            return static_cast<uint16_t>(port);
        }

    public:
        uint16_t mPortBase;
        uint16_t mDomainIDGain;
        uint16_t mParticipantIDGain;
        uint16_t mOffsetd0;
        uint16_t mOffsetd1;
        uint16_t mOffsetd2;
        uint16_t mOffsetd3;
    };
}

#endif