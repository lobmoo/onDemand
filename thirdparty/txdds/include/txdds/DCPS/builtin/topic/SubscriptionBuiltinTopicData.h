/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-24 20:09:47
 * @FilePath     : /TXDDS/include/txdds/DCPS/builtin/topic/SubscriptionBuiltinTopicData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_DCPS_SubscriptionBuiltinTopicData_H
#define TXDDS_DCPS_SubscriptionBuiltinTopicData_H

#include "txdds/DCPS/builtin/topic/BuiltinTopicKey_t.h"
#include "txdds/DCPS/core/policy/QosPolicy.h"

namespace BaoSky::dds::builtin
{
    struct SubscriptionBuiltinTopicData
    {
        BuiltinTopicKey_t key;
        BuiltinTopicKey_t participant_key;
        std::string topic_name;
        std::string type_name;
        DurabilityQosPolicy durability;
        DeadlineQosPolicy deadline;
        LatencyBudgetQosPolicy latency_budget;
        LivelinessQosPolicy liveliness;
        ReliabilityQosPolicy reliability;
        OwnershipQosPolicy ownership;
        DestinationOrderQosPolicy destination_order;
        UserDataQosPolicy user_data;
        TimeBasedFilterQosPolicy time_based_filter;
        PresentationQosPolicy presentation;
        PartitionQosPolicy partition;
        TopicDataQosPolicy topic_data;
        GroupDataQosPolicy group_data;
        DurabilityServiceQosPolicy durability_service;
        LifespanQosPolicy lifespan;
    };
}

#endif