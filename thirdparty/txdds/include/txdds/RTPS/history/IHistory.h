/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/txdds/RTPS/history/IHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_IHISTORY_H
#define TXDDS_RTPS_IHISTORY_H

#include "txdds/RTPS/common/SequenceNumber.h"
#include "txdds/RTPS/common/Guid.h"
#include "txdds/RTPS/attributes/HistoryAttributes.h"
#include "txdds/RTPS/history/CacheChange.h"

#include <mutex>

namespace BaoSky::rtps
{
    using HistoryIterator = std::vector<CacheChange *>::const_iterator;

    inline bool OrderHistory(const CacheChange *lhs, const CacheChange *rhs)
    {
        return lhs->writerGuid == rhs->writerGuid ? lhs->sequenceNumber < rhs->sequenceNumber : lhs->sourceTimestamp < rhs->sourceTimestamp;
    }

    class IEndpoint;

    class TXDDS_API IHistory
    {
    public:
        IHistory(const HistoryAttributes &hatt) : mAttributes(hatt), mOwner(nullptr) {}

        virtual ~IHistory()
        {
        }

        virtual bool ReceiveChange(CacheChange *change) = 0;

        virtual bool RemoveChange(CacheChange *change) = 0;

        virtual bool GetChange(CacheChange *&change, SequenceNumber seq, const BaoSky::rtps::Guid &guid = GUID_UNKNOWN) = 0;

        virtual void SetHistoryOwner(IEndpoint *endpoint) { mOwner = endpoint; }

        virtual HistoryIterator begin() { return mChanges.begin(); }

        virtual HistoryIterator end() { return mChanges.end(); }

        virtual std::recursive_mutex &GetMutex() { return mChangeMutex; }

        virtual bool Empty() { return mChanges.empty(); }

        virtual uint32_t GetPayloadMaxSize() { return mAttributes.mPayloadMaxSize; }

        virtual SequenceNumber GetLatestSequence(BaoSky::rtps::Guid guid)
        {
            SequenceNumber seq = SEQUENCENUMBER_UNKNOWN;
            if (mChanges.empty())
            {
                return seq;
            }
            for (int i = mChanges.size() - 1; i >= 0; i--)
            {
                if (mChanges[i]->writerGuid == guid)
                {
                    seq = mChanges[i]->sequenceNumber;
                    break;
                }
            }
            return seq;
        }

        virtual bool RemoveChangeNoLock(CacheChange *ch) = 0;

        virtual HistoryAttributes GetAttribute()
        {
            return mAttributes;
        }
        HistoryAttributes mAttributes;

        virtual void MarkDelete()
        {
            std::lock_guard<std::recursive_mutex> lock(mChangeMutex);
            for (auto it : mChanges)
            {
                it->data_value.needFree = true;
            }
        }

    protected:
        IEndpoint *mOwner;

        std::vector<CacheChange *> mChanges;

        std::recursive_mutex mChangeMutex;
    };
}

#endif