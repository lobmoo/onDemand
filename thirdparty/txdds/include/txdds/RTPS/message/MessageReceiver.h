/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-31 15:22:09
 * @FilePath: /TXDDS/include/RTPS/message/MessageReceiver.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_MESSAGERECEIVE_H
#define TXDDS_RTPS_MESSAGERECEIVE_H

#include <txdds/RTPS/common/RTPSMessageTypes.h>
#include <txdds/RTPS/common/RTPSEntityTypes.h>
#include <txdds/RTPS/common/GuidPrefix.h>
#include <txdds/RTPS/common/LocatorList_t.h>
#include <txdds/RTPS/common/Parameter.h>
#include <txdds/RTPS/common/CDRMessage.h>
#include <txdds/RTPS/common/Time.h>
#include <txdds/RTPS/message/SubMessageHeader.h>
#include <txdds/RTPS/message/submessages/Data.h>
#include <txdds/RTPS/message/submessages/HeartBeat.h>
#include <txdds/RTPS/message/submessages/AckNack.h>
#include <txdds/RTPS/message/submessages/Gap.h>
#include <txdds/RTPS/message/submessages/InfoDestination.h>
#include <txdds/RTPS/message/submessages/InfoTimestamp.h>
#include <txdds/RTPS/message/submessages/DataFrag.h>
#include <txdds/RTPS/message/submessages/HeartBeatFrag.h>
#include <txdds/RTPS/message/submessages/NackFrag.h>
#include <map>
#include <mutex>
#include <condition_variable>
namespace BaoSky::rtps
{
    class IParticipant;
    class IEndpoint;
    struct CacheChange;

    struct MessageReceiverData
    {
        ProtocolVersion_t sourceVersion;
        VendorId_t sourceVendorId;
        GuidPrefix sourceGuidPrefix;
        GuidPrefix destGuidPrefix;
        bool haveTimestamp;
        Time rtpsSendTimestamp;
    };

    class TXDDS_API MessageReceiver
    {
    protected:
        MessageReceiverData mData;
        CDRMessage *mCdrMsg;
        IParticipant *mParticipant;
        std::map<EntityId, IEndpoint *> mReaders;
        std::map<EntityId, IEndpoint *> mWriters;
        GuidPrefix mPrefix;
        bool mOpFlag;
        std::mutex mOpFlagMtx;
        std::condition_variable mOpFlagCV;

        /**
         * @Description : 根据 mCdrMsg 解析 RTPS 头，并将 guid 前缀、协议版本、供应商等信息存入 message receive
         * @return       {bool} 解析是否成功
         */
        virtual bool ProcessRTPSHeader();

        /**
         * @Description : 根据 mCdrMsg 解析 SubmessageHeader，包含submessageId、flag、messageLength等信息
         * @param        header：output param
         * @return       {bool} 解析是否成功
         */
        virtual bool ProcessSubMessageHeader(SubmessageHeader &header);

        /**
         * @Description : 根据 mCdrMsg 以及 SubmessageHeader 解析 DataMsg，其中包含readerId、writerId、payload等信息
         * @param        header: input param
         * @param        data: output param
         * @return       {bool} 解析是否成功
         */
        virtual bool ProcessDataMsg(SubmessageHeader header, DataMsg &data, CacheChange *ch);

        /**
         * @Description : 根据 mCdrMsg 以及 SubmessageHeader 解析 HeartBeatMsg
         * @param        header: input param
         * @param        data: output param
         * @return       {bool} 解析是否成功
         */
        virtual bool ProcessHeartBeatMsg(SubmessageHeader header, HeartBeatMsg &data);

        /**
         * @Description : 根据 mCdrMsg 以及 SubmessageHeader 解析 AckNackMsg
         * @param        header: input param
         * @param        data: output param
         * @return       {bool} 解析是否成功
         */
        virtual bool ProcessAckNackMsg(SubmessageHeader header, AckNackMsg &data);

        /**
         * @Description : 根据 mCdrMsg 以及 SubmessageHeader 解析 GapMsg
         * @param        header: input param
         * @param        data: output param
         * @return       {bool} 解析是否成功
         */
        virtual bool ProcessGapMsg(SubmessageHeader header, GapMsg &data);
        virtual bool ProcessInfoDST(SubmessageHeader header, InfoDestinationMsg &data);
        virtual bool ProcessInfoTimestamp(SubmessageHeader header, InfoTimestampMsg &data);
        virtual bool ProcessNackFragMsg(SubmessageHeader header, NackFragMsg &data);
        virtual bool ProcessHeartBeatFragMsg(SubmessageHeader header, HeartBeatFragMsg &data);
        virtual bool ProcessDataFragMsg(SubmessageHeader header, DataFragMsg &data, CacheChange *ch);
        virtual bool ProcessDataMsgInlineQos(BaoSky::rtps::CDRMessage &cdrMsg, CacheChange *ch);
        virtual void NotifyData(EntityId id, CacheChange *ch);
        virtual void NotifyGap(GapMsg gapMsg);
        virtual void NotifyHeartBeat(HeartBeatMsg heartBeatMsg);
        virtual void NotifyAckNack(AckNackMsg ackMsg);
        virtual void NotifyDataFrag(CacheChange *ch, DataFragMsg datafrag);
        virtual void NotifyHeartBeatFrag(HeartBeatFragMsg heartbeat);
        virtual void NotifyNackFrag(NackFragMsg nack);

    public:
        virtual uint32_t Count()
        {
            return mReaders.size() + mWriters.size();
        }
        MessageReceiver(IParticipant *participant = nullptr);
        virtual void OnDataReceive(CDRMessage *cdrMsg);
        virtual void AddEndPoint(IEndpoint *endpoint);
        virtual void RemoveEndPoint(IEndpoint *endpoint);
        virtual ~MessageReceiver();
        std::mutex mMtx;
        std::map<GuidPrefix, uint32_t> mMapPrefix;
    };
}

#endif
