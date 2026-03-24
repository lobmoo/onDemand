/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/txdds/RTPS/reader/IStatefulReader.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_ISTATEFULREADER_H
#define TXDDS_RTPS_ISTATEFULREADER_H

#include "txdds/RTPS/reader/IReader.h"
#include "txdds/RTPS/reader/WriterProxy.h"
#include "txdds/RTPS/timedEvent/TimedEventService.h"

namespace BaoSky::rtps
{
    class TXDDS_API IStatefulReader : public IReader
    {
    public:
        IStatefulReader(
            const BaoSky::rtps::Guid &guid,
            IHistory *history,
            IParticipant *participant,
            ReaderAttributes watt,
            IReaderListener *listener = nullptr)
            : IReader(guid, history, participant, watt, listener) {}

        virtual ~IStatefulReader() override {}

        virtual TimedEventService *GetEventResource() = 0;

        /**
         * @Description : 根据记录的远端 writer 信息发送 acknack 报文
         * @param        writerProxy: input param
         * @return       {bool} acknack 是否发送成功
         */
        virtual bool SendAckNack(WriterProxy &writerProxy) = 0;
        virtual Duration GetHeartBeatDelayTime() { return mAttributes.heartbeatResponseDelay; }

        virtual bool CancelHBEvent() = 0;

        virtual bool StartHBEvent() = 0;
    };
}

#endif