/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-02-06 15:01:09
 * @FilePath     : /TXDDS/include/DCPS/subscriber/history/DataReaderHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-20 14:32:14
 * @FilePath: /TXDDS/include/DCPS/subscriber/history/DataReaderHistory.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_DATAREADERHISTORY_H
#define TXDDS_DCPS_DATAREADERHISTORY_H
#include <txdds/DCPS/topic/TypeSupport.h>
#include <txdds/DCPS/subscriber/qos/DataReaderQos.h>
#include <txdds/RTPS/history/ReaderHistory.h>

namespace BaoSky::dds
{
    class ITopic;
    class DataReaderHistory : public rtps::ReaderHistory
    {
        friend class DataReader;

    private:
        HistoryQosPolicy history_;
        ITopic *mTopic;
        ResourceLimitsQosPolicy resource_limited_qos_;
        std::map<InstanceHandle, std::vector<BaoSky::rtps::CacheChange *>> mInstances;
        virtual bool remove_change_inner(BaoSky::rtps::CacheChange *ch);
        virtual bool received_change_keep_all(BaoSky::rtps::CacheChange *ch);
        virtual bool received_change_keep_last(BaoSky::rtps::CacheChange *ch);
        virtual bool check_instance(BaoSky::rtps::CacheChange *ch);
        virtual void add_instance(BaoSky::rtps::CacheChange *ch);
        virtual bool add_change(BaoSky::rtps::CacheChange *ch);
        virtual bool can_be_add(BaoSky::rtps::CacheChange *ch);
        static DataReaderHistory *CreateInstance(TypeSupport type, ITopic *topic, DataReaderQos qos);
        // not implement,used to mock object
        DataReaderHistory();
        DataReaderHistory(const TypeSupport &type, ITopic *topic, const DataReaderQos &qos);

        virtual ~DataReaderHistory();

        virtual bool remove_change(BaoSky::rtps::CacheChange *ch);

        virtual bool ReceiveChange(BaoSky::rtps::CacheChange *ch);
    };

}
#endif