/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-07-01 13:38:51
 * @FilePath     : /TXDDS/include/txdds/RTPS/message/submessages/HeartBeat.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */

#ifndef _TXDDS_RTPS_HEARTBEATMSG_H_
#define _TXDDS_RTPS_HEARTBEATMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
namespace BaoSky::rtps
{
    struct HeartBeatMsg
    {
        bool FinalFlag;
        bool LivelinessFlag;
        bool GroupInfoFlag;
        EntityId readerId;
        EntityId writerId;
        SequenceNumber firstSN;
        SequenceNumber lastSN;
        Count count;
        SequenceNumber currentGSN;
        SequenceNumber firstGSN;
        SequenceNumber lastGSN;
        GroupDigest writerSet;
        GroupDigest secureWriterSet;
        HeartBeatMsg()
        {
            FinalFlag = false;
            LivelinessFlag = false;
            GroupInfoFlag = false;
            memset(&writerSet, 0, 4);
            memset(&secureWriterSet, 0, 4);
            count = 0;
            lastSN = SEQUENCENUMBER_UNKNOWN;
            firstSN = SEQUENCENUMBER_UNKNOWN;
            currentGSN = SEQUENCENUMBER_UNKNOWN;
            firstGSN = SEQUENCENUMBER_UNKNOWN;
            lastGSN = SEQUENCENUMBER_UNKNOWN;
        }
    };

}
#endif