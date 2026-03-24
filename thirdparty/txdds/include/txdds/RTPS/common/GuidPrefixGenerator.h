/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-10 10:52:43
 * @FilePath: /TXDDS/include/RTPS/common/GuidPrefixGenerator.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_GUIDPREFIXGENERATOR_T_H
#define TXDDS_RTPS_GUIDPREFIXGENERATOR_T_H
#include <random>
#include <limits>

#include "GuidPrefix.h"
#include "txdds/RTPS/common/RTPSEntityTypes.h"
#include "txdds/RTPS/utils/SystemInfo.h"
namespace BaoSky::rtps
{
    class GuidPrefixGenerator
    {
        using octet = unsigned char;

    public:
        GuidPrefix GetGuidPrefix() const
        {
            return mGuidPrefix;
        }
        static const GuidPrefixGenerator &GetInstance()
        {
            static GuidPrefixGenerator instance;
            return instance;
        }
        void GenerateGuidPrefix(uint32_t parId, GuidPrefix &prefix) const
        {
            std::copy(mGuidPrefix.mValue, mGuidPrefix.mValue + 8, prefix.mValue);
            prefix.mValue[8] = static_cast<octet>(parId & 0xff);
            prefix.mValue[9] = static_cast<octet>((parId >> 8) & 0xff);
            prefix.mValue[10] = static_cast<octet>((parId >> 16) & 0xff);
            prefix.mValue[11] = static_cast<octet>((parId >> 24) & 0xff);
        }
        void GenerateCustomGuidPrefix(uint32_t parId, GuidPrefix &prefix) const
        {
            prefix.mValue[8] = static_cast<octet>(parId & 0xff);
            prefix.mValue[9] = static_cast<octet>((parId >> 8) & 0xff);
            prefix.mValue[10] = static_cast<octet>((parId >> 16) & 0xff);
            prefix.mValue[11] = static_cast<octet>((parId >> 24) & 0xff);
        }

    private:
        GuidPrefixGenerator()
        {
            // todo:get host id
            mGuidPrefix.mValue[0] = VENDORID_TXDDS[0];
            mGuidPrefix.mValue[1] = VENDORID_TXDDS[1];
            uint16_t hostId = 1;
            mGuidPrefix.mValue[2] = static_cast<octet>(hostId & 0xff);
            mGuidPrefix.mValue[3] = static_cast<octet>((hostId >> 8) & 0xff);
            int processId = SystemInfo::GetInstance().GetProcessId();
            mGuidPrefix.mValue[4] = static_cast<octet>(processId & 0xff);
            mGuidPrefix.mValue[5] = static_cast<octet>((processId >> 8) & 0xff);
            std::random_device randomGenerator;
            std::uniform_int_distribution<uint16_t> distribution(0, std::numeric_limits<uint16_t>::max());
            uint16_t value = distribution(randomGenerator);
            mGuidPrefix.mValue[6] = static_cast<octet>(value & 0xff);
            mGuidPrefix.mValue[7] = static_cast<octet>((value >> 8) & 0xff);
        }
        GuidPrefix mGuidPrefix;
    };
}

#endif