/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/RTPS/transport/TransportDescription.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_THREADCONFIG
#define TXDDS_RTPS_THREADCONFIG

#include <string>
#include <vector>

namespace BaoSky::rtps
{
    struct ThreadConfig
    {
        std::string mConfigName = "";
        std::string mThreadName = "";
        std::vector<uint16_t> mCPUSet{};
        int mPriority = 0;
        int mSchedPolicy = 0;

        bool operator<(const ThreadConfig &other) const
        {
            return mConfigName < other.mConfigName;
        }
    };
}

#endif