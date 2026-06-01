/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/txdds/RTPS/attributes/EndpointAttributes.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_ENDPOINTATTRIBUTES_H
#define TXDDS_RTPS_ENDPOINTATTRIBUTES_H

#include "txdds/RTPS/common/RTPSEntityTypes.h"
#include "txdds/RTPS/common/LocatorList_t.h"
#include <txdds/DCPS/core/policy/QosPolicy.h>
namespace BaoSky::rtps
{
    class EndpointAttributes
    {
    public:
        EndpointAttributes() = default;

        virtual ~EndpointAttributes() = default;

        TopicKind_t topicKind = TopicKind_t::NO_KEY;

        ReliabilityKind_t reliabilityLevel = ReliabilityKind_t::BEST_EFFORT;

        LocatorList_t unicastLocatorList;

        LocatorList_t multicastLocatorList;

        BaoSky::dds::DurabilityQosPolicyKind durabilityKind;

        EndpointKind_t endpointKind = EndpointKind_t::WRITER;

        int16_t mMaxMatchedLimit = -1;

        BaoSky::rtps::MemoryManagementPolicy mHistoryMemoryPolicy = BaoSky::rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;

        bool mIsSHMDirect = false;
    };
}

#endif