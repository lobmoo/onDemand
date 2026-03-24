/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 14:49:30
 * @FilePath: /TXDDS/include/RTPS/builtin/data/DiscoveredParticipantData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_DiscoveredParticipantData_H
#define TXDDS_RTPS_DiscoveredParticipantData_H
#include "txdds/RTPS/builtin/data/ParticipantProxy.h"
#include "txdds/DCPS/builtin/topic/ParticipantBuiltinTopicData.h"
#include "txdds/RTPS/common/CDRMessage.h"
#include "txdds/RTPS/common/Guid.h"
#include "txdds/RTPS/common/InstanceHandle.h"
#include "txdds/RTPS/builtin/data/DiscoveredWriterData.h"
#include "txdds/RTPS/builtin/data/DiscoveredReaderData.h"

#include <unordered_map>
using ParticipantBuiltinTopicData = BaoSky::dds::builtin::ParticipantBuiltinTopicData;

namespace BaoSky::rtps
{
    class TXDDS_API DiscoveredParticipantData
    {
    public:
        DiscoveredParticipantData();
        virtual ~DiscoveredParticipantData();
        void Reset();
        bool WriteCDRMessage(CDRMessage &cdrMsg, bool isWriteEncapsulation);
        void CopyProperty(const DiscoveredParticipantData *data);
        bool ReadFromCDRMessage(CDRMessage *msg, bool isReadEncapsulation, bool isUseShmTransport, bool isFilterLocator, VendorId_t sourceVendor = VENDORID_TXDDS);
        bool UpdateDiscoveredParticipantData(const DiscoveredParticipantData &newData);
        void UpdateLocator(const DiscoveredParticipantData &newData);
        inline ParticipantProxy &GetParticipantProxy()
        {
            return mParticipantProxy;
        }

        bool mIsCheckLeaseDuration;
        bool mIsAlive;
        bool mIsAdaptiveAnnouncement = false;
        BaoSky::rtps::Guid mParticipantGuid;
        Duration mLeaseDuration;
        InstanceHandle mInstanceHandle;
        std::chrono::steady_clock::time_point mLastMessageTime;
        std::unordered_map<EntityId, std::shared_ptr<DiscoveredReaderData>> mDiscoveredReaderData;
        std::unordered_map<EntityId, std::shared_ptr<DiscoveredWriterData>> mDiscoveredWriterData;

    private:
        ParticipantBuiltinTopicData mParticipantBuiltinTopicData;
        ParticipantProxy mParticipantProxy;
    };
}

#endif