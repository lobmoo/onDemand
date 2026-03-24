/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-05 14:32:32
 * @FilePath: /TXDDS/include/RTPS/RTPSDomain.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_RTPSDOMAIN_H
#define TXDDS_RTPS_RTPSDOMAIN_H

#include "txdds/RTPS/attributes/ParticipantAttributes.h"
#include "txdds/RTPS/common/Guid.h"
#include <txdds/RTPS/common/EntityId.h>
#include "txdds/txddsexport.h"
namespace BaoSky::rtps
{
    class IParticipant;
    class IParticipantListener;

    class TXDDS_API RTPSDomain
    {
    public:
        static IParticipant *CreateParticipant(const uint32_t &domainId, const ParticipantAttributes &patt,
                                               IParticipantListener *listen = nullptr);

        static void DeleteParticipant(IParticipant *&parti);

        static bool CreateParticipantGuid(int32_t &partiId, BaoSky::rtps::Guid &guid);
    };
}

#endif