/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2024-09-20 09:35:53
 * @FilePath     : /TXDDS/include/RTPS/message/submessages/NackFrag.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef _TXDDS_RTPS_NACKFRAGMSG_H_
#define _TXDDS_RTPS_NACKFRAGMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>

#include <vector>
namespace BaoSky::rtps
{
    struct NackFragMsg
    {
        EntityId readerId;
        EntityId writerId;
        SequenceNumber writerSN;
        FragmentNumberSet_t fragmentNumberState;
        Count count = 0;
    };
}
#endif