/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-13 16:35:15
 * @FilePath: /TXDDS/include/RTPS/writer/IStatelessWriter.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_ISTATELESSWRITER_H
#define TXDDS_RTPS_ISTATELESSWRITER_H

#include "txdds/RTPS/writer/IWriter.h"

namespace BaoSky::rtps
{
    class IStatelessWriter : public IWriter
    {
    public:
        IStatelessWriter(
            const BaoSky::rtps::Guid &guid,
            IHistory *history,
            IParticipant *participant,
            WriterAttributes watt,
            std::shared_ptr<TransportStack> transportStack,
            IWriterListener *listener = nullptr)
            : IWriter(guid, history, participant, watt, transportStack, listener) {}

        virtual ~IStatelessWriter() override {}

        virtual void ResetUnsentChanges() = 0;
    };
}

#endif