/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-07-01 13:38:51
 * @FilePath     : /TXDDS/include/txdds/DCPS/common/StatusKind.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */
#ifndef TXDDS_DCPS_STATUSKIND_H
#define TXDDS_DCPS_STATUSKIND_H

#include <cstdint>

namespace BaoSky::dds
{
    typedef uint32_t StatusKind;
    typedef uint32_t StatusMask;
    // const StatusKind INCONSISTENT_TOPIC_STATUS = 0x0001 << 0;
    // const StatusKind OFFERED_DEADLINE_MISSED_STATUS = 0x0001 << 1;
    // const StatusKind REQUESTED_DEADLINE_MISSED_STATUS = 0x0001 << 2;
    // const StatusKind OFFERED_INCOMPATIBLE_QOS_STATUS = 0x0001 << 5;
    // const StatusKind REQUESTED_INCOMPATIBLE_QOS_STATUS = 0x0001 << 6;
    // const StatusKind SAMPLE_LOST_STATUS = 0x0001 << 7;
    // const StatusKind SAMPLE_REJECTED_STATUS = 0x0001 << 8;
    // const StatusKind DATA_ON_READERS_STATUS = 0x0001 << 9;
    // const StatusKind DATA_AVAILABLE_STATUS = 0x0001 << 10;
    // const StatusKind LIVELINESS_LOST_STATUS = 0x0001 << 11;
    // const StatusKind LIVELINESS_CHANGED_STATUS = 0x0001 << 12;
    // const StatusKind PUBLICATION_MATCHED_STATUS = 0x0001 << 13;
    // const StatusKind SUBSCRIPTION_MATCHED_STATUS = 0x0001 << 14;
    const StatusKind ALL_STATUS = UINT32_MAX;
}

#endif