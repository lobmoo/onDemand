/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2026-05-13 10:19:06
 * @FilePath     : /TXDDS/include/txdds/DCPS/publisher/history/DataWriterHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_DATAWRITERHISTORY_H
#define TXDDS_DATAWRITERHISTORY_H
#include <txdds/DCPS/common/InstanceHandle.h>
#include "txdds/RTPS/history/WriterHistory.h"
namespace BaoSky::dds
{
    class ITopic;
    // class CacheChange;
    class DataWriterHistory : public BaoSky::rtps::WriterHistory
    {
    private:
        HistoryQosPolicy history_;
        ITopic *mTopic;
        ResourceLimitsQosPolicy resource_limited_qos_;
        std::map<InstanceHandle, std::vector<BaoSky::rtps::CacheChange *>> mInstances;
        uint32_t mdepth;
        bool AddToInstance(BaoSky::rtps::CacheChange *ch);

    public:
        DataWriterHistory(BaoSky::rtps::HistoryAttributes attr);

        ~DataWriterHistory();

        virtual bool ReceiveChange(BaoSky::rtps::CacheChange *change) override;

        virtual bool RemoveChange(BaoSky::rtps::CacheChange *change) override;

        virtual bool GetChange(BaoSky::rtps::CacheChange *&change, BaoSky::rtps::SequenceNumber seq, const BaoSky::rtps::Guid &guid = BaoSky::rtps::GUID_UNKNOWN) override;

        virtual bool RemoveChangeNoLock(BaoSky::rtps::CacheChange *ch) override;
    };
}
#endif