/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2026-01-22 10:56:50
 * @FilePath     : /TXDDS/include/txdds/RTPS/writer/ReaderProxy.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-15 16:28:26
 * @FilePath: /TXDDS/include/RTPS/writer/ReaderProxy.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_READERPROXY_H
#define TXDDS_RTPS_READERPROXY_H

#include "txdds/RTPS/common/Guid.h"
#include "txdds/RTPS/common/Locator.h"
#include "txdds/RTPS/common/SequenceNumber.h"
#include "txdds/RTPS/common/Duration.h"
#include "txdds/RTPS/writer/ChangeForReader.h"
#include <map>
#include <mutex>

namespace BaoSky::rtps
{
    class IStatefulWriter;
    class GapMsg;
    class ReaderProxyData;
    class ReaderProxy
    {
        friend class StatelessWriter;
        friend class StatefulWriter;
    public:
        ReaderProxy();
        virtual ~ReaderProxy();
        void SetWriter(IStatefulWriter *writer);
        bool HasAckChange(const SequenceNumber &seq);
        bool IsRequestChanges(bool &isNeedSendGap, GapMsg &gapMsg, const SequenceNumberSet &reqSeqSet, const SequenceNumber &minHistorySeq);
        void SetReaderProxyData(ReaderProxyData *rData);
        bool IsUpdateACKCount(const uint32_t &ackCount);
        void ProcessNack();
        void AddChange(const ChangeForReader &change);
        void AckSeqSet(const SequenceNumber &seqBase);
        ChangeForReader GetChange(const SequenceNumber &seq);
        void RemoveReceivedChange(const SequenceNumber &seq);

    private:
        BaoSky::rtps::Guid remoteReaderGuid;

        LocatorList_t unicastLocatorList;
        LocatorList_t multicastLocatorList;
        SequenceNumber mLowSeqRecord;
        std::map<SequenceNumber, ChangeForReader> mReceivedChanges;
        IStatefulWriter *mWriter;
        uint32_t mACKCount;
        bool mIsReliable;
        std::mutex mMtx;
    };
}

#endif