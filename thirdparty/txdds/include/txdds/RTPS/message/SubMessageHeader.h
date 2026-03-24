/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 13:10:57
 * @FilePath    : /TXDDS/include/RTPS/message/SubMessageHeader.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_SUBMESSAGE_H_
#define _TXDDS_RTPS_SUBMESSAGE_H_
#include <txdds/RTPS/message/SubMessageElements.h>

namespace BaoSky::rtps
{
    struct SubmessageHeader
    {
        SubmessageKind submessageId;
        SubmessageFlag flags;
        uint16_t octetsToNextHeader;
        SubmessageHeader()
        {
            flags = 0x00;
            octetsToNextHeader = 0;
            submessageId = RTPS_HE;
        }
    };

}
#endif