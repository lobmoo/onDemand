/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-02 10:06:09
 * @FilePath    : /TXDDS/include/DCPS/subscriber/IDataReaderListener.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_IDATAREADERLISTENER_H
#define TXDDS_DCPS_IDATAREADERLISTENER_H

#include "txdds/DCPS/core/status/SubscriptionMatchedStatus.h"

namespace BaoSky::dds
{
    class IDataReader;

    class TXDDS_API IDataReaderListener
    {
    public:
        virtual ~IDataReaderListener() {}

        virtual void OnDataAvailable(IDataReader *reader)
        {
            (void)reader;
        }

        virtual void OnSubscriptionMatched(IDataReader *reader, const SubscriptionMatchedStatus &info)
        {
            (void)reader;
            (void)info;
        }

        virtual bool OnDataAvailableImprove(uint8_t *buffer, uint32_t length)
        {
            (void)buffer;
            (void)length;
            return false;
        }
    };
}

#endif