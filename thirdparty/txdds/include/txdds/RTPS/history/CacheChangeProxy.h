/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2026-04-01 16:31:22
 * @FilePath     : /TXDDS/include/txdds/RTPS/history/CacheChangeProxy.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#include <iostream>
#include <map>
#include <vector>
#include <txdds/RTPS/common/FragmentNumber_t.h>
namespace BaoSky::rtps
{
    class CacheChangeProxy
    {
    private:
        FragmentNumber_t mbase;
        uint32_t mFragementNumber;
        uint16_t mFragementSize;
        std::map<FragmentNumber_t, uint16_t> mReceivedFragment;

    public:
        virtual bool Isfull();
        virtual bool ReceiveFragment(FragmentNumber_t fragment);
        virtual void MissingData(std::vector<FragmentNumberSet_t> &missingData);
        CacheChangeProxy();
        CacheChangeProxy(uint32_t fragmentNumber, uint16_t fragmentSize);
        ~CacheChangeProxy();
    };
}