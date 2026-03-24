/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 14:05:28
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/AckNackMsg.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_ACKNACKMSG_H_
#define _TXDDS_RTPS_ACKNACKMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>

#include <vector>
namespace BaoSky::rtps
{
    struct AckNackMsg
    {
        bool FinalFlag = 0;
        EntityId readerId;
        EntityId writerId;
        SequenceNumberSet readerSNState;
        Count count = 0;
    };
}
#endif