/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-02-06 15:01:09
 * @FilePath     : /TXDDS/include/txdds/RTPS/reader/IReader.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_IREADER_H
#define TXDDS_RTPS_IREADER_H

#include "txdds/RTPS/IEndpoint.h"
#include "txdds/RTPS/reader/IReaderListener.h"
#include "txdds/RTPS/builtin/data/WriterProxyData.h"
#include "txdds/RTPS/message/SubMessageElements.h"
#include "txdds/RTPS/attributes/ReaderAttributes.h"
#include "txdds/RTPS/history/CacheChange.h"

namespace BaoSky::rtps
{
    class IReaderListener;
    class WriterProxy;
    class TXDDS_API IReader : public IEndpoint
    {
    public:
        IReader(const BaoSky::rtps::Guid &guid, IHistory *history, IParticipant *participant,
                ReaderAttributes ratt, IReaderListener *listener = nullptr)
            : IEndpoint(guid, history, participant), mListener(listener), mAttributes(ratt), mUnRead(false)
        {
            mAttributes.endpoint.endpointKind = EndpointKind_t::READER;
        }

        virtual ~IReader() override
        {
        }

        virtual bool AddMatchedWriter(const WriterProxyData &writerProxyData) = 0;

        virtual bool RemoveMatchedWriter(const BaoSky::rtps::Guid &guid, WriterProxy *&target) = 0;

        virtual bool IsMatchedWriter(const BaoSky::rtps::Guid &guid) = 0;

        /**
         * @Description : 收到 CacheChange 后处理流程，StatelessReader 和 StatefulReader 实现不一样
         * @param        {CacheChange} *ch
         * @return       {*}
         */
        virtual void ProcessDataMsg(CacheChange *ch) = 0;

        virtual void ProcessGapMsg(const BaoSky::rtps::Guid &writerGUID, const SequenceNumber &gapStart, const SequenceNumberSet &gapList, const ChangeCount &filterCount) = 0;

        virtual void ProcessHearBeatMsg(
            const BaoSky::rtps::Guid &writerGUID,
            Count count,
            SequenceNumber firstSN,
            SequenceNumber lastSN,
            bool final,
            bool liveliness) = 0;

        virtual void ProcessHeartBeatFrag(GuidPrefix prefix, HeartBeatFragMsg msg) = 0;

        virtual void ProcessDataFrag(CacheChange *ch, uint32_t dataSize, uint16_t fragmentsInSubmessage, uint16_t fragmentSize, uint32_t fragmentStartingNum) = 0;

        virtual EndpointAttributes &GetAttributes() override { return mAttributes.endpoint; }

        virtual IReaderListener *GetReaderListener() { return mListener; }

        virtual bool ReleaseChange(CacheChange *change);
        virtual bool wait_for_unread_cache(const Duration &timeout);
        virtual void update_last_notified(const BaoSky::rtps::Guid &guid, const SequenceNumber &seq);
        virtual void ProcessBuffer(uint8_t *data, uint32_t length) = 0;

    protected:
        IReaderListener *mListener;
        std::condition_variable_any new_notification_cv_;
        ReaderAttributes mAttributes;
        bool mUnRead;
    };
}

#endif