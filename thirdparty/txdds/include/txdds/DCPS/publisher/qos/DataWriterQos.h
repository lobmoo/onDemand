/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-01-10 16:04:27
 * @FilePath     : /TXDDS/include/DCPS/publisher/qos/DataWriterQos.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */
/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-17 15:23:40
 * @FilePath: /TXDDS/include/DCPS/publisher/qos/DataWriterQos.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_DCPS_DATAWRITERQOS_H_
#define _TXDDS_DCPS_DATAWRITERQOS_H_
#include <txdds/DCPS/core/policy/QosPolicy.h>
namespace BaoSky::dds
{
    class DataWriterQos
    {
    public:
        DataWriterQos()=default;
        ReliabilityQosPolicy reliability;
        HistoryQosPolicy history;
        ResourceLimitsQosPolicy resource_limits;
        RTPSEndPointQos endpoint;
        DurabilityQosPolicy durability;

    };
}
#endif