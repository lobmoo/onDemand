/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-10 10:48:19
 * @FilePath: /TXDDS/include/RTPS/common/RTPSMessageTypes.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_RTPSMESSAGETYOE_T_H
#define TXDDS_RTPS_RTPSMESSAGETYOE_T_H
#include <cstdint>
#include <vector>
#ifndef _WIN32
#include <endian.h>
#endif // _WIN32
#include <array>
namespace BaoSky::rtps
{
    using octet = uint8_t;
    using SubmessageFlag = unsigned char;
    using Count = uint32_t;
    using GroupDigest = octet[4];
    using UExtension4 = octet[4];
    using WExtension8 = octet[8];
    typedef octet *Checksum;
    const Checksum CHECKSUN_INVALID = nullptr;
    enum Endianness
    {
        BIGEND = 0x1,
        LITTLEEND = 0x0
    };
#if defined _WIN32
    constexpr Endianness DEFAULT_ENDIAN = LITTLEEND; // MSVC LITTLE ONLY
#else
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN
    constexpr Endianness DEFAULT_ENDIAN = LITTLEEND;
#else
    constexpr Endianness DEFAULT_ENDIAN = BIGEND;
#endif
#endif
    enum ProtocolId
    {
        PROTOCOL_RTPS
    };

    enum SubmessageKind : octet
    {
        RTPS_HE = 0x00,
        PAD = 0x01,
        ACKNACK = 0x06,
        HEARTBEAT = 0x07,
        GAP = 0x08,
        INFO_TS = 0x09,
        INFO_SRC = 0x0c,
        INFO_REPLY_IP4 = 0x0d,
        INFO_DST = 0x0e,
        INFO_REPLY = 0x0f,
        NACK_FRAG = 0x12,
        HEARTBEAT_FRAG = 0x13,
        DATA = 0x15,
        DATA_FRAG = 0x16
    };

    enum ParameterId : uint16_t
    {
        PID_PAD = 0x0000,
        PID_SENTINEL = 0x0001,
        PID_USER_DATA = 0x002c,
        PID_TOPIC_NAME = 0x0005,
        PID_TYPE_NAME = 0x0007,
        PID_GROUP_DATA = 0x002d,
        PID_TOPIC_DATA = 0x002e,
        PID_DURABILITY = 0x001d,
        PID_DURABILITY_SERVICE = 0x001e,
        PID_DEADLINE = 0x0023,
        PID_LATENCY_BUDGET = 0x0027,
        PID_LIVELINESS = 0x001b,
        PID_RELIABILITY = 0x001a,
        PID_LIFESPAN = 0x002b,
        PID_DESTINATION_ORDER = 0x0025,
        PID_HISTORY = 0x0040,
        PID_RESOURCE_LIMITS = 0x0041,
        PID_OWNERSHIP = 0x001f,
        PID_OWNERSHIP_STRENGTH = 0x0006,
        PID_PRESENTATION = 0x0021,
        PID_PARTITION = 0x0029,
        PID_TIME_BASED_FILTER = 0x0004,
        PID_TRANSPORT_PRIORITY = 0x0049,
        PID_DOMAIN_ID = 0x000f,
        PID_DOMAIN_TAG = 0x4014,
        PID_PROTOCOL_VERSION = 0x0015,
        PID_VENDOR_ID = 0x0016,
        PID_UNICAST_LOCATOR = 0x002f,
        PID_MULTICAST_LOCATOR = 0x0030,
        PID_DEFAULT_UNICAST_LOCATOR = 0x0031,
        PID_DEFAULT_MULTICAST_LOCATOR = 0x0048,
        PID_METATRAFFIC_UNICAST_LOCATOR = 0x0032,
        PID_METATRAFFIC_MULTICAST_LOCATOR = 0x0033,
        PID_EXPECTS_INLINE_QOS = 0x0043,
        PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT = 0x0034,
        PID_PARTICIPANT_LEASE_DURATION = 0x0002,
        PID_CONTENT_FILTER_PROPERTY = 0x0035,
        PID_PARTICIPANT_GUID = 0x0050,
        PID_GROUP_GUID = 0x0052,
        PID_GROUP_ENTITYID = 0x0053,
        PID_BUILTIN_ENDPOINT_SET = 0x0058,
        PID_BUILTIN_ENDPOINT_QOS = 0x0077,
        PID_PROPERTY_LIST = 0x0059,
        PID_TYPE_MAX_SIZE_SERIALIZED = 0x0060,
        PID_ENTITY_NAME = 0x0062,
        PID_ENDPOINT_GUID = 0x005a,
        PID_CONTENT_FILTER_INFO = 0x0055,
        PID_COHERENT_SET = 0x0056,
        PID_DIRECTED_WRITE = 0x0057,
        PID_ORIGINAL_WRITER_INFO = 0x0061,
        PID_GROUP_COHERENT_SET = 0x0063,
        PID_GROUP_SEQ_NUM = 0x0064,
        PID_WRITER_GROUP_INFO = 0x0065,
        PID_SECURE_WRITER_GROUP_INFO = 0x0066,
        PID_KEY_HASH = 0x0070,
        PID_STATUS_INFO = 0x0071,
        PID_TYPE_IDV1 = 0x0069,
        PID_TYPE_OBJECTV1 = 0x0072,
        PID_DATA_REPRESENTATION = 0x0073,
        PID_TYPE_CONSISTENCY_ENFORCEMENT = 0x0074,
        PID_TYPE_INFORMATION = 0x0075,
        PID_IDENTITY_TOKEN = 0x1001,
        PID_PERMISSIONS_TOKEN = 0x1002,
        PID_PARTICIPANT_SECURITY_INFO = 0x1005,
        PID_ENDPOINT_SECURITY_INFO = 0x1004,
        PID_IDENTITY_STATUS_TOKEN = 0x1006,
        PID_DATA_TAGS = 0x1003
    };

}

#endif