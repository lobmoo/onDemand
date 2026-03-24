/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-15 16:28:26
 * @FilePath: /TXDDS/include/RTPS/reader/WriterProxy.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_WRITERPROXY_H
#define TXDDS_RTPS_WRITERPROXY_H

#include "txdds/RTPS/common/Guid.h"
#include "txdds/RTPS/common/Locator.h"
#include "txdds/RTPS/common/SequenceNumber.h"
#include "txdds/RTPS/common/ChangeCount_t.h"
#include "txdds/RTPS/history/CacheChange.h"
#include "txdds/RTPS/builtin/data/WriterProxyData.h"
#include <map>
#include <txdds/RTPS/message/submessages/HeartBeat.h>
#include <txdds/RTPS/builtin/data/WriterProxyData.h>
#include <txdds/RTPS/timedEvent/ScheduledEvent.h>
#include <txdds/RTPS/message/submessages/NackFrag.h>
namespace BaoSky::rtps
{
    class IStatefulReader;
    enum ChangesFromWriteStatusKind
    {
        UNKNOWN,
        MISSING,
        RECEIVED,
        NA_FILTERED,
        NA_REMOVED,
        NA_UNSPECIFIED
    };

    class ChangesFromWrite
    {
    public:
        ChangesFromWrite() : status(UNKNOWN), isRelevant(false) {}
        ChangesFromWriteStatusKind status;
        bool isRelevant;
    };

    class TXDDS_API WriterProxy
    {
        friend class StatefulReader;

    public:
        static WriterProxy *CreateInstance(IStatefulReader *reader);

        /**
         * @Description : 初始化 writerproxy 信息
         * @param        attribute: input param 包含远端 guid、 locator 等信息
         * @param        base: input param 用户可获得的最大 sequence
         * @return       {void}
         */
        virtual void init(WriterProxyData attribute, SequenceNumber base);

        virtual ~WriterProxy();

        /**
         * @Description : 获取用户可获得的最大 sequence
         * @return       {SequenceNumber}
         */
        virtual SequenceNumber AvailableChangesMax();

        /**
         * @Description : 根据 gap 中不可获取的 sequence 更新远端信息
         * @param        a_seq_num_seq: input param 不可获取的 sequence 集合
         * @param        filteredCount: input param a_seq_num_seq 中仍存在远端 history 的 sequence 数量
         * @return       {void}
         */
        virtual void NotAvailableChangeSet(SequenceNumberSet seqNumSet, ChangeCount_t filteredCount);

        /**
         * @Description : 所有小于 seqNum 的数据将被认为不可获取，并更新远端信息
         * @param        seqNum: input param 所有小于 seqNum 的数据将被认为不可获取
         * @param        changes_remove: input param 如果这些 sequence 不存在远端 history 中，则为 true。在功能实现中无实际用途
         * @return       {void}
         */
        virtual void LostChangesUpdate(const SequenceNumber &seqNum, const bool &changes_remove = true);

        /**
         * @Description : 获取从上一次更新后缺失的 sequence 集合
         * @return       {SequenceNumberSet}
         */
        virtual SequenceNumberSet MissingChanges();

        /**
         * @Description : 更新远端的 sequence 最大值
         * @param        seqNum: input param
         * @return       {void}
         */
        virtual void MissingChangesUpdate(const SequenceNumber &seqNum);

        /**
         * @Description : 接收到数据后更新远端信息
         * @param        seqNum: input param
         * @return       {void}
         */
        virtual void ReceivedChangeSet(const SequenceNumber &seqNum);

        /**
         * @Description : 接收到数据但是判断数据不可用后，更新远端信息
         * @param        seqNum: input param
         * @return       {void}
         */
        virtual void IrrelevantChangeSet(const SequenceNumber &seqNum);

        /**
         * @Description : 判断是否是已经接受过的 sequence
         * @param        seqNum: input param
         * @return       {bool}
         */
        virtual bool IsReceivedMessage(SequenceNumber seqNum);

        /**
         * @Description : 获取下一个能够通知给用户的 sequence 最小值
         * @return       {SequenceNumber}
         */
        virtual SequenceNumber NextSeqToNotified();
        virtual inline void SetWriterProxyData(WriterProxyData *wData)
        {
            mRemoteWriterGuid = wData->GetGuid();
            for (auto it = wData->GetUnicastLocator().begin(); it != wData->GetUnicastLocator().end(); it++)
            {
                unicastLocatorList.push_back(*it);
            }
            for (auto it = wData->GetMulticastLocator().begin(); it != wData->GetMulticastLocator().end(); it++)
            {
                multicastLocatorList.push_back(*it);
            }
            mDataMaxSizeSerialized = wData->GetDataMaxSize();
        }

        virtual void UpDateHeartBeatDelay(Duration time);

        virtual bool CancelHBEvent();

        virtual bool StartHBEvent();

    private:
        WriterProxy(IStatefulReader *reader);
        void UpdateSeqNumber(const SequenceNumber &seqNum, ChangesFromWriteStatusKind status, bool irrelevant = false);

        virtual bool ProcessHeartBeat(Count count, SequenceNumber firstSN, SequenceNumber lastSN, bool final, bool liveliness);

        virtual bool ProcessHeartBeatFrag(Count count, SequenceNumber seq, FragmentNumber lastFragmentNum);

        void Clear();

        virtual bool SendAckNack();

        BaoSky::rtps::Guid mRemoteWriterGuid;

        LocatorList_t unicastLocatorList;

        LocatorList_t multicastLocatorList;

        long mDataMaxSizeSerialized;

        std::map<SequenceNumber, ChangesFromWrite> mReceivedChanges;

        SequenceNumber mLastAvailableSeqNum;

        SequenceNumber mBaseSeqNum;

        SequenceNumber mLastNotifySeqNum;

        Count mHeartBeatCount;
        Count mHeartBeatFragCount;
        IStatefulReader *mReader;

        ScheduledEvent *mHeartBeatEvent;

        bool mFinalFlag;

        bool mAlive;
    };
}

#endif