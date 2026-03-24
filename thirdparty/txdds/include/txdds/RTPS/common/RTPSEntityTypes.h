/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-10 11:09:44
 * @FilePath: /TXDDS/include/RTPS/common/RTPSEntityTypes.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_RTPSENTITYTYPES_H
#define TXDDS_RTPS_RTPSENTITYTYPES_H

#include <cstdint>
#include <array>
#include <cstring>

namespace BaoSky::rtps
{
    using BuiltinEndpointQos_t = uint32_t;
    using BuiltinEndpointSet_t = uint32_t;
    using NetworkConfigSet_t = uint32_t;
    using Count = uint32_t;
    using DomainId = uint32_t;

    enum class ReliabilityKind_t : uint32_t
    {
        RELIABLE,
        BEST_EFFORT
    };

    enum class TopicKind_t
    {
        NO_KEY,
        WITH_KEY
    };

    enum class EndpointKind_t
    {
        READER,
        WRITER
    };

    enum class ChangeKind
    {
        ALIVE,
        ALIVE_FILTERED,
        NOT_ALIVE_DISPOSED,
        NOT_ALIVE_UNREGISTERED,

        NOT_ALIVE_DISPOSED_UNREGISTERED
    };

    using VendorId_t = std::array<uint8_t, 2>;
    const VendorId_t VENDORID_UNKNOWN = {0X00, 0X00};
    // todo:vendor id?
    const VendorId_t VENDORID_TXDDS = {0x01, 0x01};

    struct ProtocolVersion_t
    {
        unsigned char mMajor;
        unsigned char mMinor;
        // todo:HAVE_SECURITY
        ProtocolVersion_t() : ProtocolVersion_t(2, 4)
        {
        }
        ProtocolVersion_t(unsigned char major, unsigned char minor) : mMajor(major), mMinor(minor)
        {
        }

        bool operator==(const ProtocolVersion_t &version) const
        {
            return mMajor == version.mMajor && mMinor == version.mMinor;
        }

        bool operator!=(const ProtocolVersion_t &version) const
        {
            return mMajor != version.mMajor || mMinor != version.mMinor;
        }
    };
    const ProtocolVersion_t PROTOCOLVERSION_1_0{1, 0};
    const ProtocolVersion_t PROTOCOLVERSION_1_1{1, 1};
    const ProtocolVersion_t PROTOCOLVERSION_2_0{2, 0};
    const ProtocolVersion_t PROTOCOLVERSION_2_1{2, 1};
    const ProtocolVersion_t PROTOCOLVERSION_2_2{2, 2};
    const ProtocolVersion_t PROTOCOLVERSION_2_4{2, 4};
    const ProtocolVersion_t PROTOCOLVERSION{2, 5};
}

#endif