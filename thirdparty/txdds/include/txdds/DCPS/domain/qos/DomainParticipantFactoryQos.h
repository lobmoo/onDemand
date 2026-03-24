/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-17 15:12:31
 * @FilePath: /TXDDS/include/DCPS/domain/qos/DomainParticipantFactoryQos.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_DCPS_DOMAINPARTICIPANTFACTORYQOS_H_
#define _TXDDS_DCPS_DOMAINPARTICIPANTFACTORYQOS_H_

#include "txdds/DCPS/core/policy/QosPolicy.h"

namespace BaoSky::dds
{
    class TXDDS_API DomainParticipantFactoryQos
    {
    public:
        DomainParticipantFactoryQos() = default;

        ~DomainParticipantFactoryQos() = default;

    };
}
#endif