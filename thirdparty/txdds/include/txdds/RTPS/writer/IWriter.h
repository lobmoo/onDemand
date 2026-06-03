/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/txdds/RTPS/writer/IWriter.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_IWRITER_H
#define TXDDS_RTPS_IWRITER_H

#include "txdds/RTPS/IEndpoint.h"
#include "txdds/RTPS/common/SerializedPayload.h"
#include "txdds/RTPS/common/InstanceHandle.h"
#include "txdds/RTPS/attributes/WriterAttributes.h"
#include "txdds/RTPS/transport/SendResource.h"

namespace BaoSky::rtps
{
    const uint32_t CDRMSG_BASIC_LENGTH = 512;
    const uint32_t CDRMSG_MAX_LENGTH = 65535;

    struct CacheChange;
    class ReaderProxy;
    class AckNackMsg;
    class IWriterListener;

    class IWriter : public IEndpoint
    {
    public:
        IWriter(
            const BaoSky::rtps::Guid &guid,
            IHistory *history,
            IParticipant *participant,
            WriterAttributes watt,
            IWriterListener *listener = nullptr)
            : IEndpoint(guid, history, participant), mAttributes(watt), mListener(listener), mMsg(CDRMSG_BASIC_LENGTH * 2)
        {
            mAttributes.endpoint.endpointKind = EndpointKind_t::WRITER;
        }

        virtual ~IWriter() override {}

        virtual EndpointAttributes &GetAttributes() override { return mAttributes.endpoint; }

        /**
         * @Description : 将 payload 新建为 CacheChange
         * @param        {ChangeKind} &kind
         * @param        {SerializedPayload} &data
         * @param        {ParameterList} inlineQos
         * @param        {InstanceHandle} handle
         * @return       {*}
         */
        virtual CacheChange *NewChange(
            const ChangeKind &kind,
            SerializedPayload &data,
            bool isNeedCopy,
            ParameterList inlineQos = ParameterList(),
            InstanceHandle handle = INSTANCEHANDLE_UNKNOWN) = 0;

        /**
         * @Description : 发送 CacheChange
         * @param        {CacheChange} *ch
         * @return       {*}
         */
        virtual bool SendChange(CacheChange *ch) = 0;

        virtual bool AddMatchedReader(std::shared_ptr<ReaderProxy> readerProxy) = 0;

        virtual bool RemoveMatchedReader(const ReaderProxy &readerProxy) = 0;
        virtual bool IsMatchedReader(const BaoSky::rtps::Guid &guid) = 0;
        virtual bool ReplyAckNack(const BaoSky::rtps::Guid &writerGuid, const BaoSky::rtps::Guid &readerGuid, const AckNackMsg &msg) = 0;

        virtual bool ReplyNackFrag(const BaoSky::rtps::Guid &writerGuid, const BaoSky::rtps::Guid &readerGuid, const NackFragMsg &msg) = 0;

        virtual IWriterListener *GetWriterListener() { return mListener; }

        virtual bool AllSend(CDRMessage &msg, const LocatorList &locatorList)
        {
            bool isSend = true;
            for (auto &sendResource : mSenderList)
            {
                isSend &= std::get<1>(sendResource)->Send(msg.buffer, msg.length, locatorList);
            }
            return isSend;
        }

        virtual void RemoveChangeByKind(ChangeKind kind, const InstanceHandle &handle) = 0;

        virtual bool SendBuffer(uint8_t *buffer, uint32_t length) = 0;

    protected:
        virtual void CheckCdrMsg(CDRMessage &msg, uint32_t payloadLength)
        {
            auto totalLength = payloadLength + CDRMSG_BASIC_LENGTH;
            if (totalLength > CDRMSG_MAX_LENGTH)
            {
                if (msg.max_size != CDRMSG_MAX_LENGTH)
                {
                    msg.Resize(CDRMSG_MAX_LENGTH);
                }
            }
            else if (totalLength > msg.max_size)
            {
                msg.Resize((totalLength + 3) & ~3);
            }
            msg.pos = 0;
            msg.length = 0;
        }

        WriterAttributes mAttributes;

        IWriterListener *mListener;

        CDRMessage mMsg;
    };
}

#endif