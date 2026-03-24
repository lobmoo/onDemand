/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/txdds/RTPS/transport/BasicChannel.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_BASICCHANNEL
#define TXDDS_RTPS_BASICCHANNEL
#include <atomic>
#include <algorithm>
#include <mutex>
#include <list>

namespace BaoSky::rtps
{
    class RecvResource;

    class BasicChannel
    {
    public:
        BasicChannel() : mRecvResource(nullptr) { mAlive.store(true); }

        virtual ~BasicChannel() = default;

        virtual void SetRecvResource(RecvResource *resource)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mRecvResource = resource;
        }

        virtual void DeleteRecvResource(RecvResource *resource)
        {
            (void)resource;
            std::lock_guard<std::mutex> lock(mMutex);
            mRecvResource = nullptr;
        }

        bool Alive()
        {
            return mAlive.load();
        }

    protected:
        std::mutex mMutex;
        std::atomic<bool> mAlive;
        RecvResource *mRecvResource;
    };
}

#endif