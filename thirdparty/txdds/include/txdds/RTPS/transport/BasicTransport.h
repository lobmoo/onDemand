/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/RTPS/transport/TransportDescription.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_BASICTRANSPORT
#define TXDDS_RTPS_BASICTRANSPORT

#include "txdds/RTPS/transport/TransportConfig.h"
#include "txdds/RTPS/common/Locator.h"

#include <asio/asio.hpp>

#include <memory>
#include <map>

namespace BaoSky::rtps
{
    class SendResource;
    class RecvResource;

    class BasicTransport
    {
    public:
        BasicTransport() {}

        virtual ~BasicTransport() = default;

        virtual bool Initial() = 0;

        virtual void Final() = 0;

        virtual SendResource *CreateSendResource(const Locator &locator, const std::shared_ptr<BasicTransportConfig> &config) = 0;

        virtual void DeleteSendResource(SendResource *&resource, const Locator &locator, const std::shared_ptr<BasicTransportConfig> &config) = 0;

        virtual RecvResource *CreateRecvResource(const Locator &locator, const std::shared_ptr<BasicTransportConfig> &config) = 0;

        virtual void DeleteRecvResource(RecvResource *&resource, const Locator &locator, const std::shared_ptr<BasicTransportConfig> &config) = 0;

    protected:
        std::mutex mMutex;

        eTransportKind mKind;
    };
}

#endif