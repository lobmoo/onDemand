/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-02-06 15:01:09
 * @LastEditors  : Please set LastEditors
 * @LastEditTime : 2025-07-08 09:35:16
 * @FilePath     : /TXDDS/include/txdds/DCPS/subscriber/IDataReader.h
 * @Description  : 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-02 10:06:09
 * @FilePath: /TXDDS/include/DCPS/subscriber/IDataReader.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */

#ifndef TXDDS_DCPS_IDATAREADER_H
#define TXDDS_DCPS_IDATAREADER_H

#include <txdds/DCPS/core/IDomainEntity.h>
#include <txdds/DCPS/subscriber/SampleInfo.h>
#include <txdds/DCPS/subscriber/qos/DataReaderQos.h>
#include <txdds/RTPS/common/Guid.h>
namespace BaoSky::dds
{
    class IDataReaderListener;

    class TXDDS_API IDataReader : public IDomainEntity
    {
    public:
        IDataReader() : IDomainEntity() {}

        virtual ~IDataReader() override {}

        virtual ReturnCode Enable() = 0;

        virtual ReturnCode Disable() = 0;

        virtual ReturnCode SetListener(IDataReaderListener *listener, const StatusMask &mask = ALL_STATUS) = 0;

        virtual IDataReaderListener *GetListener() = 0;

        virtual ReturnCode SetQos(const DataReaderQos &qos) = 0;

        virtual ReturnCode GetQos(DataReaderQos &qos) = 0;

        virtual ReturnCode TakeNextSample(void *sample, SampleInfo info) = 0;

        virtual std::string GetTopicName() = 0;
        //  仅用于测试，不是很严谨
        virtual bool wait_for_unread_cache(const Duration &timeout) = 0;
        //  仅用于测试，不是很严谨
        virtual void update_last_notified(const rtps::Guid &guid, const rtps::SequenceNumber &seq) = 0;
    };
}

#endif