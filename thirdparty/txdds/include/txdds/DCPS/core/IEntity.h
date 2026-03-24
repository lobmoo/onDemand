/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-02 17:20:58
 * @FilePath    : /TXDDS/include/DCPS/core/IEntity.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */

#ifndef TXDDS_DCPS_IENTITY_H
#define TXDDS_DCPS_IENTITY_H

#include "txdds/DCPS/common/InstanceHandle.h"
#include "txdds/DCPS/common/StatusKind.h"
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/txddsexport.h"

#include <mutex>

namespace BaoSky::dds
{
    class IListener;

    class TXDDS_API IEntity
    {
    public:
        IEntity() : mStatusMask(ALL_STATUS), mHandle(), mEnable(false) {}

        virtual ~IEntity() = default;

        virtual ReturnCode Enable() = 0;

        virtual ReturnCode Disable() = 0;

        virtual InstanceHandle GetInstanceHandle()
        {
            return mHandle;
        }

        virtual void SetInstanceHandle(const InstanceHandle &handle)
        {
            mHandle = handle;
        }

        virtual StatusMask GetStatusMask()
        {
            return mStatusMask;
        }

        virtual void SetStatusMask(const StatusMask &mask)
        {
            mStatusMask = mask;
        }

    protected:
        StatusMask mStatusMask;

        InstanceHandle mHandle;

        bool mEnable;

        std::mutex mMtx;
    };
}

#endif