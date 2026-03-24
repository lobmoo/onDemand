/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-17 15:23:11
 * @FilePath: /TXDDS/include/DCPS/publisher/qos/PublisherQos.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_DCPS_PUBLISHERQOS_H_
#define _TXDDS_DCPS_PUBLISHERQOS_H_
#include <txdds/DCPS/core/policy/QosPolicy.h>
namespace BaoSky::dds
{
    class PublisherQos
    {
    public:
        PublisherQos() {}
        GroupDataQosPolicy group_data;
        PresentationQosPolicy presentation;
        PartitionQosPolicy partition;
        EntityFactoryQosPolicy entity_factory;
    };
}
#endif