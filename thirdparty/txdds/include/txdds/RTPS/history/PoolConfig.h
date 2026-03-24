/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-03-07 13:40:23
 * @FilePath     : /TXDDS/TXDDS/include/RTPS/history/PoolConfig.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_POOLCONFIG_H_
#define TXDDS_RTPS_POOLCONFIG_H_

#include <txdds/RTPS/attributes/HistoryAttributes.h>
#include <txdds/RTPS/resources/ResourceManagement.h>
namespace BaoSky::rtps
{
    struct PoolConfig
    {
        PoolConfig()
        {
        }

        constexpr PoolConfig(MemoryManagementPolicy_t policy, uint32_t payloadSize, uint32_t iniSize, uint32_t maxSize) noexcept
            : mMemoryPolicy(policy), mPayloadInitialSize(payloadSize), mInitialSize(iniSize), mMaximumSize(maxSize)
        {
        }

        MemoryManagementPolicy_t mMemoryPolicy = PREALLOCATED_MEMORY_MODE;

        uint32_t mPayloadInitialSize = 0;

        uint32_t mInitialSize = 0;

        uint32_t mMaximumSize = 0;

        static constexpr PoolConfig FromHistoryAttributes(const HistoryAttributes &historyAttr) noexcept
        {
            // todo pool的数量限制由什么决定
            return {
                historyAttr.mMemoryPolicy,
                historyAttr.mPayloadMaxSize,
                static_cast<uint32_t>(historyAttr.mInitialReservedCaches <= 0 ? 0 : historyAttr.mInitialReservedCaches + historyAttr.mExtraReservedCaches + 500),
                static_cast<uint32_t>(historyAttr.mMaximumReservedCaches <= 0 ? 0 : historyAttr.mMaximumReservedCaches + historyAttr.mExtraReservedCaches + 500)};
        };
    };
}

#endif
