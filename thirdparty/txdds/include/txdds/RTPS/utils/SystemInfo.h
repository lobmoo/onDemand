/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-05-20 13:55:25
 * @FilePath: /TXDDS/include/RTPS/utils/SystemInfo.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */
#ifndef TXDDS_RTPS_SystemInfo_H
#define TXDDS_RTPS_SystemInfo_H
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif // if defined(_WIN32)

#include <cstdint>
namespace BaoSky::rtps
{
    class SystemInfo
    {
    public:
        inline int GetProcessId() const
        {
#if defined(__cplusplus_winrt)
            return (int)GetCurrentProcessId();
#elif defined(_WIN32)
            return (int)_getpid();
#else
            return (int)getpid();
#endif
        }

        inline uint16_t GetHostId() const
        {
            return 0;
        }

        static const SystemInfo &GetInstance()
        {
            static SystemInfo systemInfoInstance;
            return systemInfoInstance;
        }
    };
}
#endif