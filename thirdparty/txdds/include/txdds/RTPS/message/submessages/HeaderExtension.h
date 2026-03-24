/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-22 09:34:25
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/HeaderExtension.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_HEADEREXTENSIONMSG_H_
#define _TXDDS_RTPS_HEADEREXTENSIONMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>

#include <vector>
namespace BaoSky::rtps
{
    struct HeaderExtensionMsg
    {
        bool LengthFlag = false;
        bool TimestampFlag = false;
        bool UExtensionFlag = false;
        bool WExtensionFlag = false;
        bool ChecksumFlags = false;
        bool ParametersFlag = false;
        MessageLength messageLength;
        Timestamp rtpsSendTimestamp;
        UExtension4 uExtension4;
        WExtension8 wExtension8_t;
        Checksum messageChexksum;
        ParameterList parameters;
    };
}
#endif