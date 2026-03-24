/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 16:36:17
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/InfoDestination.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_INFODESTINATIONMSG_H_
#define _TXDDS_RTPS_INFODESTINATIONMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
namespace BaoSky::rtps
{
    struct InfoDestinationMsg
    {
        GuidPrefix guidPrefix;
        InfoDestinationMsg()
        {
            guidPrefix = GUIDPREFIX_UNKNOWN;
        }
    };
}
#endif