/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-01-04 14:32:59
 * @FilePath     : /TXDDS/include/RTPS/message/submessages/DataFrag.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef _TXDDS_RTPS_DATAFRAGMSG_H_
#define _TXDDS_RTPS_DATAFRAGMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
#include <txdds/RTPS/common/SerializedPayload.h>
namespace BaoSky::rtps
{
    struct DataFragMsg
    {
        bool InlineQosFlag = false;
        bool NonStandardPayloadFlag = false;
        bool KeyFlag = false;
        EntityId readerId;
        EntityId writerId;
        SequenceNumber writerSN;
        // 第几个片段开始
        FragmentNumber fragmentStartingNum = 0;
        // 片段数
        uint16_t fragmentsInSubmessage = 1;
        // 总长
        uint32_t dataSize = 0;
        // 片段长度
        uint16_t fragmentSize = 0;
        ParameterList InlineQos;
        SerializedData serializedPayload;
    };

}
#endif