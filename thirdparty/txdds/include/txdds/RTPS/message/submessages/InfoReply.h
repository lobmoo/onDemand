/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 16:41:30
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/InfoReply.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_INFOREPLYMSG_H_
#define _TXDDS_RTPS_INFOREPLYMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
namespace BaoSky::rtps
{
    struct InfoReplyMsg
    {
        bool MulticastFlag = false;
        LocatorList unicastLocatorList;
        LocatorList multicastLocatorList;
    };

}
#endif