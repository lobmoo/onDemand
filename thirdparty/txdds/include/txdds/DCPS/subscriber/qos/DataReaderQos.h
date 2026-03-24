/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-02-06 15:01:09
 * @FilePath     : /TXDDS/include/txdds/DCPS/subscriber/qos/DataReaderQos.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */

#ifndef _TXDDS_DCPS_DATAREADERQOS_H_
#define _TXDDS_DCPS_DATAREADERQOS_H_
#include <txdds/DCPS/core/policy/QosPolicy.h>
namespace BaoSky::dds
{

    class DataReaderQos
    {
    public:
        DataReaderQos() {}
        ReliabilityQosPolicy reliability;
        HistoryQosPolicy history;
        ResourceLimitsQosPolicy resource_limits;
        RTPSEndPointQos endpoint;
        DurabilityQosPolicy durability;
        
        
    };
    extern const DataReaderQos DATAREADER_QOS_DEFAULT;
}
#endif