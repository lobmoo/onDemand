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

}

#endif