/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-03-19 17:25:22
 * @FilePath     : /TXDDS/include/RTPS/common/PayloadInfo.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef RTPS_COMMON_PAYLOADINFO_H_
#define RTPS_COMMON_PAYLOADINFO_H_

#include <txdds/RTPS/history/CacheChange.h>
#include <txdds/RTPS/common/SerializedPayload.h>

#include <cassert>

namespace BaoSky::rtps
{
    struct PayloadInfo
    {
        SerializedPayload payload;

        IPayloadPool *payload_owner = nullptr;

        ~PayloadInfo()
        {
            // Avoid payload.data to be freed
            payload.data = nullptr;
            payload.length = 0;
        }

        void move_from_change(CacheChange &change)
        {
            if (payload_owner == nullptr && payload.data == nullptr && payload.length == 0)
            {
                payload_owner = change.payloadOwner;
                change.payloadOwner = nullptr;
                payload = change.data_value;
                change.data_value.data = nullptr;
                change.data_value.length = 0;
                change.data_value.pos = 0;
                change.data_value.max_size = 0;
            }
        }

        void move_into_change(CacheChange &change)
        {
            if (change.payloadOwner == nullptr)
            {
                change.payloadOwner = payload_owner;
                payload_owner = nullptr;
                change.data_value = payload;
                payload.data = nullptr;
                payload.length = 0;
                payload.pos = 0;
                payload.max_size = 0;
            }
        }
    };

}
#endif