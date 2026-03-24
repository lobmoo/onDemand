/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-03-07 13:59:00
 * @FilePath     : /TXDDS/include/txdds/RTPS/history/CacheChangePool.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_CACHECHANGEPOOL_H_
#define TXDDS_RTPS_CACHECHANGEPOOL_H_

#include "txdds/RTPS/history/CacheChange.h"
#include "txdds/RTPS/history/IChangePool.h"
#include "txdds/RTPS/resources/ResourceManagement.h"
#include "txdds/RTPS/history/PoolConfig.h"

#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace BaoSky::rtps
{
    class CacheChangePool : public IChangePool
    {
    public:
        template <class UnaryFunction>
        CacheChangePool(const PoolConfig &config, UnaryFunction f)
        {
            init(config);
            std::for_each(mAllCaches.begin(), mAllCaches.end(), f);
        }
        CacheChangePool(const PoolConfig &config);
        bool ReserveCache(CacheChange *&cachechange) override;
        bool ReleaseCache(CacheChange *cachechange) override;
        size_t GetAllCachesSize();
        size_t GetFreeCachesSize();
        virtual ~CacheChangePool();
    protected:
        CacheChangePool() = default;
        void init(const PoolConfig &config);
        virtual CacheChange *CreateChange() const;
        virtual void DestroyChange(CacheChange *change) const;

    private:
        uint32_t mCurrentPoolSize = 0;
        uint32_t mMaxPoolSize = 0;
        MemoryManagementPolicy_t mMemoryMode = MemoryManagementPolicy_t::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
        std::vector<CacheChange *> mFreeCaches;
        std::vector<CacheChange *> mAllCaches;
        bool AllocateGroup(uint32_t num_caches);
        CacheChange *AllocateSingle();
        void ReturnCacheToPool(CacheChange *ch);
    };

}

#endif
