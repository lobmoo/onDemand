/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/RTPS/transport/SendResource.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_SENDRESOURCE
#define TXDDS_RTPS_SENDRESOURCE

#include "txdds/RTPS/common/LocatorList_t.h"
#include <mutex>
#include <list>
#include <algorithm>

namespace BaoSky::rtps
{
    class BasicChannel;

    class SendResource
    {
    public:
        virtual ~SendResource() = default;

        virtual bool Send(octet *data, const uint32_t &length, const LocatorList_t &locatorList) = 0;

        inline virtual void AddChannel(std::shared_ptr<BasicChannel> channel)
        {
            std::unique_lock<std::mutex> lock(mChannelLock);
            mChannels.push_back(channel);
        }

        inline virtual void DeleteChannel(std::shared_ptr<BasicChannel> channel)
        {
            auto iter = std::find_if(
                mChannels.begin(), mChannels.end(), [&](std::shared_ptr<BasicChannel> other)
                { return channel == other; });
            mChannels.erase(iter);
        }
        inline virtual void DeleteChannel(BasicChannel *channel)
        {
            auto iter = std::find_if(
                mChannels.begin(), mChannels.end(), [&](std::shared_ptr<BasicChannel> other)
                { return channel == other.get(); });
            mChannels.erase(iter);
        }

        inline virtual std::shared_ptr<BasicChannel> GetChannel(BasicChannel *channel)
        {
            auto iter = std::find_if(
                mChannels.begin(), mChannels.end(), [&](std::shared_ptr<BasicChannel> other)
                { return channel == other.get(); });
            return *iter;
        }

    protected:
        std::list<std::shared_ptr<BasicChannel>> mChannels;
        std::mutex mChannelLock;
    };
}

#endif