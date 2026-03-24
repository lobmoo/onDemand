/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/RTPS/transport/RecvResource.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_RECVRESOURCE
#define TXDDS_RTPS_RECVRESOURCE

#include "txdds/RTPS/message/MessageReceiver.h"

#include <mutex>
#include <memory>

namespace BaoSky::rtps
{
    class IEndpoint;
    class CDRMessage;
    class BasicChannel;

    class RecvResource
    {
    public:
        RecvResource() : mChannel(nullptr) {}

        virtual ~RecvResource() = default;

        virtual void OnDataReceived(CDRMessage &msg) = 0;

        virtual void SetEndpoint(IEndpoint *endpoint)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mReceiver.AddEndPoint(endpoint);
        }

        virtual void RemoveEndpoint(IEndpoint *endpoint)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mReceiver.RemoveEndPoint(endpoint);
        }

    protected:
        std::mutex mMutex;

        BasicChannel *mChannel;

        MessageReceiver mReceiver;
    };
}

#endif