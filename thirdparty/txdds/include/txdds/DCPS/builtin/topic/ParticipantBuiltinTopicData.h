/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 12:50:07
 * @FilePath: /TXDDS/include/DCPS/builtin/topic/ParticipantBuiltinTopicData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_DCPS_ParticipantBuiltinTopicData_H
#define TXDDS_DCPS_ParticipantBuiltinTopicData_H

#include "txdds/DCPS/builtin/topic/BuiltinTopicKey_t.h"
#include "txdds/DCPS/core/policy/QosPolicy.h"

namespace BaoSky::dds::builtin
{
    class ParticipantBuiltinTopicData
    {
    public:
        ParticipantBuiltinTopicData() = default;
        virtual ~ParticipantBuiltinTopicData() = default;
        BuiltinTopicKey_t key;
        UserDataQosPolicy user_data;
    };
}

#endif