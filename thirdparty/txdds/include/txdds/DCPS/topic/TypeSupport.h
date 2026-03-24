/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-06-16 15:41:29
 * @FilePath     : /TXDDS/include/DCPS/topic/TypeSupport.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_DCPS_TYPESUPPORT_H
#define TXDDS_DCPS_TYPESUPPORT_H

#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/topic/TopicDataType.h"

#include <memory>

namespace BaoSky::rtps
{
    class SerializedPayload;
}

namespace BaoSky::dds
{
    class IDomainParticipant;

    class TXDDS_API TypeSupport : public std::shared_ptr<TopicDataType>
    {
    public:
        TypeSupport() = default;

        virtual ~TypeSupport() = default;

        TypeSupport(TopicDataType *ptr);

        virtual ReturnCode RegisterType(IDomainParticipant *participant);

        virtual bool Serialize(void *data, BaoSky::rtps::SerializedPayload *payload);

        virtual bool Deserialize(BaoSky::rtps::SerializedPayload *payload, void *data);

        virtual const std::string &GetTypeName();

        inline bool operator==(const TypeSupport &typeSupport)
        {
            return get()->mTypeSize == typeSupport->mTypeSize && get()->mTypeName == typeSupport->mTypeName;
        }

        inline bool operator!=(const TypeSupport &typeSupport)
        {
            return !(*this == typeSupport);
        }
    };
}

#endif