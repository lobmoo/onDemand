/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-02 10:06:09
 * @FilePath: /TXDDS/include/DCPS/subscriber/ISubscriber.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */

#ifndef TXDDS_DCPS_ISUBSCRIBER_H
#define TXDDS_DCPS_ISUBSCRIBER_H

#include <txdds/DCPS/core/IDomainEntity.h>
#include <txdds/DCPS/subscriber/qos/SubscriberQos.h>
#include <txdds/DCPS/subscriber/qos/DataReaderQos.h>

namespace BaoSky::rtps
{
    class IParticipant;
}

namespace BaoSky::dds
{
    class ISubscriberListener;
    class ITopic;
    class IDataReader;
    class IDataReaderListener;

    class TXDDS_API ISubscriber : public IDomainEntity
    {
    public:
        ISubscriber() : IDomainEntity() {}

        virtual ~ISubscriber() override {}

        virtual ReturnCode Enable() = 0;

        virtual ReturnCode Disable() = 0;

        virtual ReturnCode SetListener(ISubscriberListener *listener, const StatusMask &mask = ALL_STATUS) = 0;

        virtual ISubscriberListener *GetListener() = 0;

        virtual ReturnCode SetQos(const SubscriberQos &qos) = 0;

        virtual ReturnCode GetQos(SubscriberQos &qos) = 0;

        virtual IDataReader *CreateDataReader(ITopic *topic, DataReaderQos qos, IDataReaderListener *listener, StatusMask mask = ALL_STATUS) = 0;

        virtual ReturnCode DeleteDataReader(IDataReader *&reader) = 0;

        virtual BaoSky::rtps::IParticipant *GetRTPSParticipant() = 0;

        virtual bool HasReader() = 0;
    };
}

#endif