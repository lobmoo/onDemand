/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 12:53:38
 * @FilePath: /TXDDS/include/RTPS/builtin/data/ParticipantProxy.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_ParticipantProxy_H
#define TXDDS_RTPS_ParticipantProxy_H

#include "txdds/RTPS/common/RTPSEntityTypes.h"
#include "txdds/RTPS/common/GuidPrefix.h"
#include "txdds/RTPS/common/LocatorList_t.h"

namespace BaoSky::rtps
{
    class TXDDS_API ParticipantProxy
    {
    public:
        ParticipantProxy() = default;
        virtual ~ParticipantProxy() = default;

        inline void Reset()
        {
            domainId = 0;
            domainTag = "";
            protocolVersion = PROTOCOLVERSION;
            guidPrefix = GUIDPREFIX_UNKNOWN;
            vendorId = VENDORID_UNKNOWN;
            expectsInlineQos = false;
            metatrafficUnicastLocatorList.Clear();
            metatrafficmulticastLocatorList.Clear();
            defaultUnicastLocatorList.Clear();
            defaultMulticastLocatorList.Clear();
            initialPeerList.Clear();
            availableBuiltinEndpoints = 0;
            manualLivelinessCount = 0;
            builtinEndpointQos = 0;
            mEntityName = "";
        }
        DomainId domainId;
        std::string domainTag;
        ProtocolVersion_t protocolVersion;
        GuidPrefix guidPrefix;
        VendorId_t vendorId;
        bool expectsInlineQos = false;
        LocatorList_t metatrafficUnicastLocatorList;
        LocatorList_t metatrafficmulticastLocatorList;
        LocatorList_t defaultUnicastLocatorList;
        LocatorList_t defaultMulticastLocatorList;
        LocatorList_t initialPeerList;
        BuiltinEndpointSet_t availableBuiltinEndpoints;
        Count manualLivelinessCount;
        BuiltinEndpointQos_t builtinEndpointQos;
        std::string mEntityName = "";
    };
}

#endif