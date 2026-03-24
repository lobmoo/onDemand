/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-02-06 15:01:09
 * @FilePath     : /TXDDS/include/DCPS/publisher/IPublisher.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_DCPS_IPUBLISHER_H
#define TXDDS_DCPS_IPUBLISHER_H

#include "txdds/DCPS/core/IDomainEntity.h"
#include "txdds/DCPS/publisher/qos/PublisherQos.h"
#include "txdds/DCPS/publisher/qos/DataWriterQos.h"

namespace BaoSky::rtps
{
    class IParticipant;
}

namespace BaoSky::dds
{
    class IPublisherListener;
    class ITopic;
    class IDataWriter;
    class IDataWriterListener;

    class TXDDS_API IPublisher : public IDomainEntity
    {
    public:
        IPublisher() : IDomainEntity() {}

        virtual ~IPublisher() override {}

        virtual ReturnCode Enable() = 0;

        virtual ReturnCode Disable() = 0;

        virtual ReturnCode SetListener(IPublisherListener *listener, const StatusMask &mask = ALL_STATUS) = 0;

        virtual IPublisherListener *GetListener() = 0;

        virtual ReturnCode SetQos(const PublisherQos &qos) = 0;

        virtual ReturnCode GetQos(PublisherQos &qos) = 0;

        virtual IDataWriter *CreateDataWriter(ITopic *topic, const DataWriterQos &qos, IDataWriterListener *listener = nullptr, const StatusMask &mask = ALL_STATUS) = 0;

        virtual ReturnCode DeleteDataWriter(IDataWriter *&writer) = 0;

        virtual BaoSky::rtps::IParticipant *GetRTPSParticipant() = 0;

        virtual bool HasWriter() = 0;
    };
}

#endif