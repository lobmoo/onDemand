/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-03-07 15:03:18
 * @FilePath     : /TXDDS/include/RTPS/history/IPayloadPool.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_IPAYLOADPOOL_H
#define TXDDS_RTPS_IPAYLOADPOOL_H
#include <txdds/RTPS/common/SerializedPayload.h>
#include <cstdint>
#include <memory>
namespace BaoSky::rtps
{
    struct CacheChange;
    class TXDDS_API IPayloadPool
    {
    public:
        virtual ~IPayloadPool() = default;
        virtual bool get_payload(uint32_t size, CacheChange &cache_change) = 0;
        virtual bool get_payload(SerializedPayload &data, IPayloadPool *&data_owner, CacheChange &cache_change) = 0;
        virtual bool release_payload(CacheChange &cache_change) = 0;
    };
}
#endif
