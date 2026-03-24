/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 14:32:41
 * @FilePath: /TXDDS/include/RTPS/builtin/data/SPDPdiscoveredParticipantData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_SPDPdiscoveredParticipantData_H
#define TXDDS_RTPS_SPDPdiscoveredParticipantData_H
#include "txdds/RTPS/builtin/data/DiscoveredParticipantData.h"
#include "txdds/RTPS/common/Duration.h"
namespace BaoSky::rtps
{
    class TXDDS_API SPDPdiscoveredParticipantData : public DiscoveredParticipantData
    {
    public:
        SPDPdiscoveredParticipantData();
        virtual ~SPDPdiscoveredParticipantData();
        Duration leaseDuration;
    };
}

#endif