/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-12-01 10:18:29
 * @FilePath     : /TXDDS/include/txdds/RTPS/common/LocatorList_t.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_LOCATORLIST_T_H
#define TXDDS_RTPS_LOCATORLIST_T_H

#include "txdds/RTPS/common/Locator.h"

#include <vector>

namespace BaoSky::rtps
{

    class TXDDS_API LocatorList_t
    {
    public:
        LocatorList_t();
        virtual ~LocatorList_t();
        void Clear();
        uint32_t GetSize();
        std::vector<Locator>::const_iterator begin() const;
        std::vector<Locator>::const_iterator end() const;
        bool empty();
        void push_back(const Locator &loc);
        void push_back(const LocatorList_t &locList);
        std::vector<Locator> mLocators;
    };
}

#endif