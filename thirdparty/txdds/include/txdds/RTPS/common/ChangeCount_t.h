/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 14:39:52
 * @FilePath: /TXDDS/include/RTPS/common/ChangeCount_t.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_CHANGECOUNT_T_H
#define TXDDS_RTPS_CHANGECOUNT_T_H

#include <cstdint>
#include <txdds/RTPS/common/SequenceNumber.h>
namespace BaoSky::rtps
{
    using ChangeCount_t = SequenceNumber;

    const ChangeCount_t CHANGECOUNTNUMBER_UNKNOWN{-1, 0};
}

#endif