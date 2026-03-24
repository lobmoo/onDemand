/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 16:05:44
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/Gap.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_GAPMSG_H_
#define _TXDDS_RTPS_GAPMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
namespace BaoSky::rtps
{
    struct GapMsg
    {
        bool GroupInfoFlag = false;
        bool FilteredCountFlag = false;

        EntityId readerId;
        EntityId writerId;
        SequenceNumber gapStart;
        SequenceNumberSet gapList;
        SequenceNumber gapStartGSN;
        SequenceNumber gapEndGSN;
        ChangeCount filteredCount;
        GapMsg()
        {
            gapStart = SEQUENCENUMBER_UNKNOWN;
            gapStartGSN = SEQUENCENUMBER_UNKNOWN;
            gapEndGSN = SEQUENCENUMBER_UNKNOWN;
            filteredCount = CHANGECOUNTNUMBER_UNKNOWN;
        }
    };

}
#endif