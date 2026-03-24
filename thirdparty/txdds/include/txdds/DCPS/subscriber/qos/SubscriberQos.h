/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-17 15:56:39
 * @FilePath: /TXDDS/include/DCPS/subscriber/qos/SubscriberQos.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_DCPS_SUBSCRIBERQOS_H_
#define _TXDDS_DCPS_SUBSCRIBERQOS_H_
#include <txdds/DCPS/core/policy/QosPolicy.h>
namespace BaoSky::dds
{
    class SubscriberQos
    {
    public:
        SubscriberQos() {}
        PartitionQosPolicy partition;
    };
    extern const SubscriberQos SUBSCRIBER_QOS_DEFAULT;
}
#endif