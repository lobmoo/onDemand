/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 12:30:14
 * @FilePath: /TXDDS/include/RTPS/builtin/entity/BuiltinEntitysDefine.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_BuiltinEntitysDefine_H
#define TXDDS_RTPS_BuiltinEntitysDefine_H
#include <cstdint>
#include <string>

namespace BaoSky::rtps
{
    static const std::string DCPS_PARTICIPANT = "DCPSParticipant";
    static const std::string DCPS_SUBSCRIPTION = "DCPSSubscription";
    static const std::string DCPS_PUBLICATION = "DCPSPublication";
    static const std::string DCPS_TOPIC = "DCPSTopic";

    struct BuiltinTopicKey_t
    {
        uint32_t mValue[3];
    };

    // BuiltinEndpointSet_t define
    constexpr uint32_t SetUint32Bit(int n)
    {
        return 1u << n;
    }
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER = SetUint32Bit(0);
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR = SetUint32Bit(1);
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER = SetUint32Bit(2);
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR = SetUint32Bit(3);
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER = SetUint32Bit(4);
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR = SetUint32Bit(5);
    constexpr uint32_t BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER = SetUint32Bit(10);
    constexpr uint32_t BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER = SetUint32Bit(11);
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_TOPICS_ANNOUNCER = SetUint32Bit(28);
    constexpr uint32_t DISC_BUILTIN_ENDPOINT_TOPICS_DETECTOR = SetUint32Bit(29);
}
#endif