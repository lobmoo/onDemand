/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 17:50:40
 * @FilePath: /TXDDS/include/RTPS/attributes/BuiltinAttributes.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_BuiltinAttributes_H
#define TXDDS_RTPS_BuiltinAttributes_H
#include <cstdint>

#include "txdds/RTPS/common/Duration.h"
#include "txdds/RTPS/common/LocatorList_t.h"
#include <chrono>
#define BUILTIN_ENDPOINT_MAX_PAYLOAD 512
namespace BaoSky::rtps
{
    const uint16_t BUILTINATTRIBUTES_MAXMATCHLIMIT = 512;

    enum class PDPMode
    {
        NONE,
        SIMPLE
    };

    enum IgnoreParticipantFlag : uint32_t
    {
        NO_FILTER = 0,
        FILTER_DIFFERENT_HOST = 0x1,
        FILTER_DIFFERENT_PROCESS = 0x2,
        FILTER_SAME_PROCESS = 0x4
    };

    class DiscoveryAttributes
    {
    public:
        DiscoveryAttributes() = default;
        virtual ~DiscoveryAttributes() = default;
        Duration mLeaseDuration = {20, 0};
        Duration mAnnouncePeriod = {3, 0};
        PDPMode mPDPMode = PDPMode::SIMPLE;
        bool mIsUseSEDP = true;
        bool mIsUsePubWriterAndSubReader = true;
        bool mIsUsePubReaderAndSubWriter = true;
        IgnoreParticipantFlag mIgnoreParticipantFlag = IgnoreParticipantFlag::NO_FILTER;
        bool mIsAdaptiveAnnouncement = false;
        std::chrono::milliseconds mAdaptiveInterval{150};
        uint32_t mRespondAnnounceNum = 2;
        std::chrono::milliseconds mRespondAnnounceInterval{50};
        bool mIsEnableDiscovery = true; // false:disable discovery other participant
        void SetEnableInternalEntityMatch(bool flag)
        {
            (void)flag;
            mIsEnableInternalEntityMatch = true;
        }
        bool GetEnableInternalEntityMatch()
        {
            return mIsEnableInternalEntityMatch;
        }

    private:
        bool mIsEnableInternalEntityMatch = false; // true:enable writer and reader match in participant
    };

    class BuiltinAttributes
    {
    public:
        BuiltinAttributes() : mBuiltinReaderPayloadSize(BUILTIN_ENDPOINT_MAX_PAYLOAD), mBuiltinWriterPayloadSize(BUILTIN_ENDPOINT_MAX_PAYLOAD)
        {
        }
        DiscoveryAttributes mDiscoveryAttributes;
        LocatorList_t unicastLocatorList;
        LocatorList_t multicastLocatorList;

        uint32_t mBuiltinReaderPayloadSize;
        uint32_t mBuiltinWriterPayloadSize;

        bool mIsAvoidBuiltinMulticast = true;
        bool mIsUseWLP = true;
        BaoSky::rtps::LocatorList_t initialPeerList;

        uint16_t mMaxMatchEndpointLimit = 100;
    };
}

#endif