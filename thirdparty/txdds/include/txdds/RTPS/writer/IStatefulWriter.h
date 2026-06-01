/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-13 16:35:15
 * @FilePath: /TXDDS/include/RTPS/writer/IStatefulWriter.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_ISTATEFULWRITER_H
#define TXDDS_RTPS_ISTATEFULWRITER_H

#include "txdds/RTPS/writer/IWriter.h"

namespace BaoSky::rtps
{
    class ReaderProxy;

    class IStatefulWriter : public IWriter
    {
    public:
        IStatefulWriter(
            const BaoSky::rtps::Guid &guid,
            IHistory *history,
            IParticipant *participant,
            WriterAttributes watt,
            IWriterListener *listener = nullptr)
            : IWriter(guid, history, participant, watt, listener) {}

        virtual ~IStatefulWriter() override {}

        /**
         * @Description : 处理 ACKNACK 报文，由 Reader 收到 ACKNACK 报文后触发
         * @return       {*}
         */
        virtual bool ReplyAckNack(const BaoSky::rtps::Guid &writerGuid, const BaoSky::rtps::Guid &readerGuid, const AckNackMsg &msg) = 0;

        /**
         * @Description : 处理 NackFrag 报文，由 Reader 收到 ACKNACK 报文后触发
         * @return       {*}
         */
        virtual bool ReplyNackFrag(const BaoSky::rtps::Guid &writerGuid, const BaoSky::rtps::Guid &readerGuid, const NackFragMsg &msg) = 0;

        /**
         * @Description : 发送 CacheChange
         * @param        {CacheChange} *ch
         * @return       {*}
         */
        virtual bool SendChangetoSpecialReader(CacheChange *ch, ReaderProxy *reader) = 0;

        virtual bool CancelHBEvent() = 0;

        virtual bool StartHBEvent() = 0;
    };
}

#endif