/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-07-01 13:38:51
 * @FilePath     : /TXDDS/include/txdds/RTPS/message/RTPSMessage.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */

#ifndef _TXDDS_RTPS_RTPSMESSAGE_H_
#define _TXDDS_RTPS_RTPSMESSAGE_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
#include <txdds/RTPS/common/CDRMessage.h>
#include <txdds/RTPS/message/SubMessages.h>
namespace BaoSky::rtps
{

    struct RTPSMessageHeader
    {
        ProtocolId protocol;
        ProtocolVersion_t version;
        VendorId_t vendorId;
        GuidPrefix guidPrefix;
        RTPSMessageHeader() : protocol(PROTOCOL_RTPS), version(PROTOCOLVERSION), vendorId(VENDORID_TXDDS), guidPrefix(GUIDPREFIX_UNKNOWN)
        {
        }

    };
    class TXDDS_API RTPSMessage
    {
    public:
        RTPSMessage() = delete;
        ~RTPSMessage() = delete;
        /**
         * @Description : 将 RTPSMessageHeader 序列化为字节流并追加到 cdrMsg中
         * @param        cdrMsg: output param
         * @param        header: input param 包含协议版本、guid 前缀、供应商等信息
         * @return       {bool} 是否序列化并追加成功
         */
        static bool AddRTPSMessageHeader(CDRMessage &cdrMsg, RTPSMessageHeader &header);

        /**
         * @Description : 将 AckNackMsg 序列化为字节流并追加到 cdrMsg中
         * @param        cdrMsg: output param
         * @param        msg: input param 包含 reader 缺失的 sequence 信息
         * @return       {bool} 是否序列化并追加成功
         */
        static bool AddAckNackMsg(CDRMessage &cdrMsg, AckNackMsg &msg);

        /**
         * @Description : 将 DataMsg 序列化为字节流并追加到 cdrMsg中
         * @param        cdrMsg: output param
         * @param        msg: input param 包含数据
         * @return       {bool} 是否序列化并追加成功
         */
        static bool AddDataMsg(CDRMessage &cdrMsg, DataMsg &msg);

        /**
         * @Description : 将 GapMsg 序列化为字节流并追加到 cdrMsg中
         * @param        cdrMsg: output param
         * @param        msg: input param 包含远端 reader 不可获取的 sequence 信息
         * @return       {bool} 是否序列化并追加成功
         */
        static bool AddGapMsg(CDRMessage &cdrMsg, GapMsg &msg);

        /**
         * @Description : 将 HeartBeatMsg 序列化为字节流并追加到 cdrMsg中
         * @param        cdrMsg: output param
         * @param        msg: input param 包含 writer 端 sequence 信息
         * @return       {bool} 是否序列化并追加成功
         */
        static bool AddHeartBeatMsg(CDRMessage &cdrMsg, HeartBeatMsg &msg);
        static bool AddDataFragMsg(CDRMessage &cdrMsg, DataFragMsg &msg);
        static bool AddHeaderExtensionMsg(CDRMessage &cdrMsg, HeaderExtensionMsg &msg);
        static bool AddHeartBeatFragMsg(CDRMessage &cdrMsg, HeartBeatFragMsg &msg);
        static bool AddInfoDestinationMsg(CDRMessage &cdrMsg, InfoDestinationMsg &msg);
        static bool AddInfoReplyMsg(CDRMessage &cdrMsg, InfoReplyMsg &msg);
        static bool AddInfoSourceMsg(CDRMessage &cdrMsg, InfoSourceMsg &msg);
        static bool AddInfoTimestampMsg(CDRMessage &cdrMsg, InfoTimestampMsg &msg);
        static bool AddNackFragMsg(CDRMessage &cdrMsg, NackFragMsg &msg);

    private:
        static bool AddSubMessageHeader(CDRMessage &cdrMsg, SubmessageHeader &header);
    };
}
#endif