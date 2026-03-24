/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 14:02:52
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/Data.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_DATAMSG_H_
#define _TXDDS_RTPS_DATAMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
#include <txdds/RTPS/common/SerializedPayload.h>
namespace BaoSky::rtps
{
    struct DataMsg
    {
        // is contain inline Qos
        bool InlineQosFlag;
        // is contain serialized Data
        bool DataFlag;
        // is contain serialized Key
        bool KeyFlag;
        // is not format according to section 10
        bool NonStandardPayloadFlag;
        EntityId readerId;
        EntityId writerId;
        SequenceNumber writerSN;
        ParameterList InlineQos;
        SerializedData serializedPayload;
        DataMsg()
        {
            InlineQosFlag = false;
            DataFlag = false;
            KeyFlag = false;
            NonStandardPayloadFlag = false;
        }

    };

}
#endif