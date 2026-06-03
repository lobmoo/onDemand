/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2026-05-19 13:26:16
 * @FilePath     : /TXDDS/include/txdds/RTPS/common/InstanceHandle.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */
/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-24 16:23:48
 * @FilePath: /TXDDS/include/RTPS/common/InstanceHandle.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_INSTANCEHANDLE_H
#define TXDDS_RTPS_INSTANCEHANDLE_H

#include <array>
#include "txdds/RTPS/common/Guid.h"
#include "txdds/txddsexport.h"

namespace BaoSky::rtps
{
    using KeyHash_t = std::array<unsigned char, 16>;
    // InstanceHandle
    struct TXDDS_API InstanceHandle
    {
    public:
        InstanceHandle() = default;
        InstanceHandle(const BaoSky::rtps::Guid &guid);
        InstanceHandle &operator=(const BaoSky::rtps::Guid &guid);
        void InstanceHandle2Guid(BaoSky::rtps::Guid &guid);
        bool IsSet() const;
        void Reset();

    public:
        KeyHash_t mValue{};
        bool mIsSet = false;
    };
    const InstanceHandle INSTANCEHANDLE_UNKNOWN;

    inline bool operator<(const InstanceHandle &handle1, const InstanceHandle &handle2)
    {
        if (handle1.mIsSet)
        {
            return handle2.mIsSet && handle1.mValue < handle2.mValue;
        }
        return handle2.mIsSet;
    }

    inline bool operator==(const InstanceHandle &handle1, const InstanceHandle &handle2)
    {
        return (handle1.mIsSet == handle2.mIsSet) && (handle1.mValue == handle2.mValue);
    }

    inline bool operator!=(const InstanceHandle &handle1, const InstanceHandle &handle2)
    {
        return (handle1.mIsSet != handle2.mIsSet) || (handle1.mValue != handle2.mValue);
    }
    inline std::ostream &operator<<(
        std::ostream &output,
        const InstanceHandle &iHandle)
    {
        std::stringstream ss;
        ss << std::hex;
        for (uint8_t i = 0; i < 15; ++i)
        {
            ss << (int)iHandle.mValue[i] << ".";
        }
        ss << (int)iHandle.mValue[15] << std::dec;
        return output << ss.str();
    }
}

#endif