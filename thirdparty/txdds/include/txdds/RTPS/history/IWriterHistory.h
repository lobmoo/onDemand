/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-01-04 14:32:59
 * @FilePath     : /TXDDS/include/RTPS/history/IWriterHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-20 09:35:53
 * @FilePath: /TXDDS/include/RTPS/history/IWriterHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_IWRITERHISTORYCACHE_H
#define TXDDS_RTPS_IWRITERHISTORYCACHE_H

#include "txdds/RTPS/history/IHistory.h"

namespace BaoSky::rtps
{
    class TXDDS_API IWriterHistory : public IHistory
    {
    public:
        IWriterHistory(const HistoryAttributes &hatt) : IHistory(hatt) {}

        virtual ~IWriterHistory() {}

        virtual SequenceNumber GetSeqNumMin() = 0;

        virtual SequenceNumber GetSeqNumMax() = 0;

        virtual bool RemoveChangeNoLock(CacheChange *ch) = 0;

        virtual HistoryIterator FindChange(CacheChange *ch) = 0;
    };
}

#endif