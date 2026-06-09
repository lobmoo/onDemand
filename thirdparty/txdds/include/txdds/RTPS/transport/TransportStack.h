/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/RTPS/transport/TransportDescription.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_TRANSPORTSTACK
#define TXDDS_RTPS_TRANSPORTSTACK

#include "txdds/RTPS/transport/TransportConfig.h"
#include "txdds/RTPS/common/Locator.h"
#include "txdds/txddsexport.h"

#include <memory>
#include <map>
#include <list>
#include <mutex>

namespace BaoSky::rtps
{
    class BasicTransport;
    class SendResource;
    class RecvResource;

    inline const std::string DEFAULT_UDP_CONFIG_NAME = "TX_DF_UDP_CONF";

    class TXDDS_API TransportStack
    {
    public:
        static std::shared_ptr<TransportStack> GetInstance();

        TransportStack();

        virtual ~TransportStack();

        void AddConfig(std::shared_ptr<BasicTransportConfig> config);

        bool DeleteConfig(const std::string &configName);

        virtual SendResource *CreateSendResource(const Locator &locator);

        virtual void DeleteSendResource(SendResource *&resource, const Locator &locator);

        virtual RecvResource *CreateRecvResource(const Locator &locator);

        virtual void DeleteRecvResource(RecvResource *&resource, const Locator &locator);

        virtual BasicTransport *GetTransport(const eTransportKind &kind);

        virtual bool GetConfig(std::shared_ptr<BaoSky::rtps::BasicTransportConfig> &config, const std::string &configName);

        virtual bool RejoinUDPMulticast(const std::string &configName, const std::string &localIP);

        virtual std::vector<std::string> GetConfigList(const eTransportKind &kind);

    private:
        void InitialTransport();

        static std::shared_ptr<TransportStack> mInstance;

        static std::mutex mMutex;

        std::list<std::shared_ptr<BasicTransportConfig>> mConfigList;

        std::map<eTransportKind, std::shared_ptr<BasicTransport>> mMapTransport;
    };
}

#endif