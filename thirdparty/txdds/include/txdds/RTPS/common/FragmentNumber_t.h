/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-15 09:45:24
 * @FilePath: /TXDDS/include/RTPS/common/FragmentNumber_t.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */

#ifndef TXDDS_RTPS_FRAGMENTNUMBER_T_H
#define TXDDS_RTPS_FRAGMENTNUMBER_T_H

#include <txdds/RTPS/utils/BitmapRange.h>
#include <iostream>
#include <cstdint>
namespace BaoSky::rtps
{
    using FragmentNumber_t = uint32_t;

    using FragmentNumberSet_t = BitmapRange<FragmentNumber_t>;

    inline std::ostream &operator<<(std::ostream &output, const FragmentNumberSet_t &fns)
    {
        output << fns.base() << ":";
        fns.for_each([&](FragmentNumber_t it)
                     { output << it << "-"; });

        return output;
    }
}

#endif