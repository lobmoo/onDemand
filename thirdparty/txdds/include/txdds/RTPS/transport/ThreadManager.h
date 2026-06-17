/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/RTPS/transport/TransportDescription.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_THREADMANAGER
#define TXDDS_RTPS_THREADMANAGER

#include "txdds/RTPS/transport/ThreadConfig.h"
#include "txdds/txddsexport.h"

#include <memory>
#include <map>
#include <list>
#include <mutex>

namespace BaoSky::rtps
{
    class ThreadResource;

    inline const std::string DEFAULT_THREAD_CONFIG_NAME = "TX_DF_TH";

    class TXDDS_API ThreadManager
    {
    public:
        static std::shared_ptr<ThreadManager> GetInstance();

        virtual ~ThreadManager();

        virtual void AddResource(const ThreadConfig &config);

        virtual bool DeleteResource(const std::string &configName);

        virtual std::shared_ptr<ThreadResource> GetResource(const std::string &configName);

        virtual bool GetConfig(ThreadConfig &config, const std::string &configName);

        virtual std::vector<std::string> GetConfigList();

    private:
        ThreadManager();

        static std::shared_ptr<ThreadManager> mInstance;

        static std::mutex mMutex;

        std::map<ThreadConfig, std::shared_ptr<ThreadResource>> mMapThread;
    };
}

#endif