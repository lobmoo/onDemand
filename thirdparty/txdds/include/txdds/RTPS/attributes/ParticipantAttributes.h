/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-20 09:35:53
 * @FilePath    : /TXDDS/include/RTPS/attributes/ParticipantAttributes.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_PARTICIPANTATTRIBUTES_H
#define TXDDS_RTPS_PARTICIPANTATTRIBUTES_H

#include "txdds/RTPS/attributes/BuiltinAttributes.h"
#include "txdds/RTPS/common/GuidPrefix.h"
#include "txdds/RTPS/common/Locator.h"
#include "txdds/RTPS/common/LocatorList_t.h"
#include "txdds/RTPS/common/RTPSEntityTypes.h"
#include "txdds/RTPS/transport/ThreadConfig.h"
#include <cstdint>
#include <vector>

namespace BaoSky::rtps
{
    class ParticipantAttributes
    {
    public:
        ParticipantAttributes() = default;

        virtual ~ParticipantAttributes() = default;

        LocatorList_t defaultUnicastLocatorList;

        LocatorList_t defaultMulticastLocatorList;

        LocatorList_t initialPeerList;

        ProtocolVersion_t protocalVersion;

        VendorId_t vendorId;

        BuiltinAttributes builtinAttributes;

        GuidPrefix prefix;

        int32_t participantId = -1;

        ThreadConfig config;
        std::string mEntityName = "";
    };
}

#endif