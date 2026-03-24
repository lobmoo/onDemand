/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-01-04 14:32:59
 * @FilePath     : /TXDDS/include/RTPS/history/IReaderHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-08 10:03:49
 * @FilePath: /TXDDS/include/RTPS/history/IReaderHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_IREADERHISTORYCACHE_H
#define TXDDS_RTPS_IREADERHISTORYCACHE_H

#include "txdds/RTPS/history/IHistory.h"

namespace BaoSky::rtps
{
    class TXDDS_API IReaderHistory : public IHistory
    {
    public:
        IReaderHistory(const HistoryAttributes &hatt) : IHistory(hatt) {}

        virtual ~IReaderHistory() {}

        virtual bool CanAddHistory(CacheChange *ch) = 0;
        virtual bool RemoveChangeNoLock(CacheChange *ch) = 0;
        virtual void RemoveChangeByGuid(const BaoSky::rtps::Guid &guid) = 0;
    };
}

#endif