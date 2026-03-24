/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-22 09:08:28
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/InfoTimestamp.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_INFOTIMESTAMPMSG_H_
#define _TXDDS_RTPS_INFOTIMESTAMPMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
namespace BaoSky::rtps
{
    struct InfoTimestampMsg
    {
        bool InvalidateFlag;
        Timestamp timestamp;
        InfoTimestampMsg()
        {
            InvalidateFlag = false;
        }
    };

}
#endif