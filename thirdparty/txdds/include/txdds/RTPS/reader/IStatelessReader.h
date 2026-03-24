/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-13 16:35:15
 * @FilePath: /TXDDS/include/RTPS/reader/IStatelessReader.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_ISTATELESSREADER_H
#define TXDDS_RTPS_ISTATELESSREADER_H

#include "txdds/RTPS/reader/IReader.h"

namespace BaoSky::rtps
{
    class TXDDS_API IStatelessReader : public IReader
    {
    public:
        IStatelessReader(
            const BaoSky::rtps::Guid &guid,
            IHistory *history,
            IParticipant *participant,
            ReaderAttributes ratt,
            IReaderListener *listener = nullptr)
            : IReader(guid, history, participant, ratt, listener) {}

        virtual ~IStatelessReader() override {}

        virtual void SetTrustWriter(const EntityId &writer) = 0;
    };
}

#endif