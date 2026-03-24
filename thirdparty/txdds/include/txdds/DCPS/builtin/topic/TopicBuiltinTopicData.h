/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 18:00:53
 * @FilePath: /TXDDS/include/DCPS/builtin/topic/TopicBuiltinTopicData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_DCPS_TopicBuiltinTopicData_H
#define TXDDS_DCPS_TopicBuiltinTopicData_H

#include "txdds/DCPS/builtin/topic/BuiltinTopicKey_t.h"
#include "txdds/DCPS/core/policy/QosPolicy.h"

namespace BaoSky::dds::builtin
{
    struct TopicBuiltinTopicData
    {
        TopicBuiltinTopicData();
        BuiltinTopicKey_t key;
        std::string name;
        std::string type_name;
        DurabilityQosPolicy durability;
        DeadlineQosPolicy deadline;
        LatencyBudgetQosPolicy latency_budget;
        LivelinessQosPolicy liveliness;
        ReliabilityQosPolicy reliability;
        TransportPriorityQosPolicy transport_priority;
        LifespanQosPolicy lifespan;
        DestinationOrderQosPolicy destination_order;
        PresentationQosPolicy presentation;
        HistoryQosPolicy history;
        ResourceLimitsQosPolicy resource_limits;
        TopicDataQosPolicy topic_data;
        OwnershipQosPolicy ownership;
    };
}
#endif