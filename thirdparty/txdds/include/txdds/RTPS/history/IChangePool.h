/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-03-07 13:14:27
 * @FilePath     : /TXDDS/TXDDS/include/RTPS/history/IChangePool.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_ICHANGEPOOL_H
#define TXDDS_RTPS_ICHANGEPOOL_H

namespace BaoSky::rtps
{

    struct CacheChange;

    class IChangePool
    {
    public:
        virtual ~IChangePool() = default;

        virtual bool ReserveCache(CacheChange *&cachechange) = 0;

        virtual bool ReleaseCache(CacheChange *cachechange) = 0;
    };
}
#endif