/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-17 16:06:59
 * @FilePath: /TXDDS/include/DCPS/topic/qos/TopicQos.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_DCPS_TOPICQOS_H_
#define _TXDDS_DCPS_TOPICQOS_H_
#include <txdds/DCPS/core/policy/QosPolicy.h>
namespace BaoSky::dds
{
    class TopicQos
    {
    public:
        TopicQos() {}
        TopicDataQosPolicy topic_data;
        DurabilityQosPolicy durability;
        DurabilityServiceQosPolicy durability_service;
        DeadlineQosPolicy deadline;
        LatencyBudgetQosPolicy latency_budget;
        OwnershipQosPolicy ownership;
        LivelinessQosPolicy liveliness;
        ReliabilityQosPolicy reliability;
        TransportPriorityQosPolicy transport_priority;
        LifespanQosPolicy lifespan;
        DestinationOrderQosPolicy destination_order;
        HistoryQosPolicy history;
        ResourceLimitsQosPolicy resource_limits;
    };
}
#endif