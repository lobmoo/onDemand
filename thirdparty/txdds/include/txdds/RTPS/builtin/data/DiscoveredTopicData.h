/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 12:33:00
 * @FilePath: /TXDDS/include/RTPS/builtin/data/DiscoveredTopicData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_DiscoveredTopicData_H
#define TXDDS_RTPS_DiscoveredTopicData_H

#include "txdds/DCPS/builtin/topic/TopicBuiltinTopicData.h"
using TopicBuiltinTopicData = BaoSky::dds::builtin::TopicBuiltinTopicData;

namespace BaoSky::rtps
{
    struct DiscoveredTopicData
    {
        DiscoveredTopicData();
        virtual ~DiscoveredTopicData();
        TopicBuiltinTopicData ddsTopicData;
    };
}

#endif