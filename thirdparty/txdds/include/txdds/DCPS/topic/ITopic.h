/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-20 09:35:53
 * @FilePath: /TXDDS/include/DCPS/topic/ITopic.h
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_ITOPIC_H
#define TXDDS_DCPS_ITOPIC_H

#include "txdds/DCPS/core/IDomainEntity.h"
#include "txdds/DCPS/topic/qos/TopicQos.h"

namespace BaoSky::dds
{
    class ITopicListener;
    class IDomainParticipant;

    class TXDDS_API ITopic : public IDomainEntity
    {
    public:
        ITopic() : IDomainEntity() {}

        virtual ~ITopic() override {}

        virtual ReturnCode Enable() = 0;

        virtual ReturnCode Disable() = 0;

        virtual IDomainParticipant *GetParticipant() = 0;

        virtual ReturnCode SetQos(const TopicQos &qos) = 0;

        virtual ReturnCode GetQos(TopicQos &qos) = 0;

        virtual ReturnCode SetListener(ITopicListener *listener, const StatusMask &mask) = 0;

        virtual ITopicListener *GetListener() = 0;

        virtual const std::string GetTopicName() = 0;

        virtual const std::string GetTypeName() = 0;

        virtual bool IsReferenced() = 0;

        virtual void Reference() = 0;

        virtual void Dereference() = 0;

        virtual uint16_t GetReference() = 0;
    };
}

#endif