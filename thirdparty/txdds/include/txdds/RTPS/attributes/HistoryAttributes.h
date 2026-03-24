/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-20 09:35:53
 * @FilePath    : /TXDDS/include/RTPS/attributes/HistoryAttributes.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_HISTORYATTRIBUTES_H
#define TXDDS_RTPS_HISTORYATTRIBUTES_H

#include <cstdint>
#include <txdds/RTPS/resources/ResourceManagement.h>
#include <txdds/DCPS/core/policy/QosPolicy.h>

namespace BaoSky::rtps
{
    class HistoryAttributes
    {
    public:
        HistoryAttributes(uint32_t payloadMaxSize = 500, uint32_t initialReservedCaches = 20, uint32_t maximumReservedCaches = 100, uint32_t extraReservedCaches = 1, MemoryManagementPolicy_t memoryPolicy = PREALLOCATED_WITH_REALLOC_MEMORY_MODE)
            : mInitialReservedCaches(initialReservedCaches), mMaximumReservedCaches(maximumReservedCaches), mExtraReservedCaches(extraReservedCaches), mPayloadMaxSize(payloadMaxSize), mMemoryPolicy(memoryPolicy)
        {
        }
        virtual ~HistoryAttributes() = default;

        uint32_t mInitialReservedCaches;
        uint32_t mMaximumReservedCaches;
        uint32_t mExtraReservedCaches;

        uint32_t mPayloadMaxSize;
        MemoryManagementPolicy_t mMemoryPolicy;
    };
}

#endif