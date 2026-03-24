/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 16:13:59
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/HeartBeatFrag.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_HEARTBEATFRAGMSG_H_
#define _TXDDS_RTPS_HEARTBEATFRAGMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
namespace BaoSky::rtps
{
    struct HeartBeatFragMsg
    {
        EntityId readerId;
        EntityId writerId;
        SequenceNumber writerSN;
        FragmentNumber lastFragmentNum;
        Count count = 0;
    };

}
#endif