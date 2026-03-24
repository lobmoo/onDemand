/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-16 14:26:17
 * @FilePath: /TXDDS/include/RTPS/common/Locator.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_Locator_H
#define TXDDS_RTPS_Locator_H

#include "txdds/RTPS/utils/IPLocator.h"
#include "txdds/txddsexport.h"
#include <sstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>
namespace BaoSky::rtps
{
    using octet = unsigned char;

    enum eLocatorKind
    {
        LOCATOR_KIND_INVALID = -1,
        LOCATOR_KIND_RESERVED = 0,
        LOCATOR_KIND_UDPv4 = 1,
        LOCATOR_KIND_UDPv6 = 2,
        LOCATOR_KIND_TCPv4 = 4,
        LOCATOR_KIND_TCPv6 = 8,
        LOCATOR_KIND_SHM = 16,
    };

    void SetInvalidAddress(octet addr[]);
    void SetInvaildLocator(Locator loc);

    class TXDDS_API Locator
    {
    public:
        Locator();

        Locator(uint32_t portin);

        Locator(int32_t kind, uint32_t port);

        void SetAddress(const Locator &locator);

        unsigned char *GetAddress();

        void DisableAddress();

        bool IsAddressDefined();

        int32_t mKind;
        uint32_t mPort;
        octet mAddress[16];
        std::string mConfigName;
    };

    inline bool IsLocatorValid(const Locator &loc)
    {
        return (0 <= loc.mKind);
    }

    inline bool operator<(const Locator &loc1, const Locator &loc2)
    {
        return memcmp(&loc1, &loc2, sizeof(Locator)) < 0;
    }

    inline bool operator==(const Locator &loc1, const Locator &loc2)
    {
        if (loc1.mKind != loc2.mKind)
        {
            return false;
        }
        if (loc1.mPort != loc2.mPort)
        {
            return false;
        }
        if (!std::equal(loc1.mAddress, loc1.mAddress + 16, loc2.mAddress))
        {
            return false;
        }
        return true;
    }

    inline bool operator!=(const Locator &loc1, const Locator &loc2)
    {
        return !(loc1 == loc2);
    }

    std::ostream &operator<<(std::ostream &output, const Locator &loc);

}

#endif