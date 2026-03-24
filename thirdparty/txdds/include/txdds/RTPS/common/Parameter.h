/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-07-04 09:52:48
 * @FilePath     : /TXDDS/include/txdds/RTPS/common/Parameter.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_PARAMETER_T_H
#define TXDDS_RTPS_PARAMETER_T_H
#include <cstdint>
#include <vector>
#include "txdds/RTPS/common/CDRMessage.h"
#include "txdds/txddsexport.h"
#include <cstdlib>
#include <map>
#include <memory>
namespace BaoSky::rtps
{
    class TXDDS_API Parameter
    {
    public:
        Parameter() : mPid(PID_PAD), mLength(0)
        {
        }

        Parameter(rtps::ParameterId pid, uint16_t length) : mPid(pid), mLength(length)
        {
        }

        virtual ~Parameter() = default;

        virtual bool AddToCDRMsg(CDRMessage &cdrMsg);

        virtual bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength);

        rtps::ParameterId mPid;

        uint16_t mLength;
        typedef std::shared_ptr<Parameter> (*CreateInstanceImpl)();
        static std::map<ParameterId, CreateInstanceImpl> mParameters;
    };
}

#endif