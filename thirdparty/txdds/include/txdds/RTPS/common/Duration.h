/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-22 10:59:24
 * @FilePath: /TXDDS/include/RTPS/common/Duration.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_RTPSDURATION_H
#define TXDDS_DCPS_RTPSDURATION_H

#include "txdds/RTPS/common/Time.h"

namespace BaoSky::rtps
{
#define PARAMETER_TIME_LENGTH 8
    using Duration = Time;
}

#endif