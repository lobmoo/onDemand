/*
 * @Author      : wanglin wanglin@baosight.com
 * @Date        : 2025-02-14 16:51:26
 * @FilePath    : /TXDDS/include/DCPS/core/status/PublicationMatchedStatus.h
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_PUBLICATIONMATCHEDSTATUS_H
#define TXDDS_DCPS_PUBLICATIONMATCHEDSTATUS_H

#include "txdds/DCPS/core/status/MatchedStatus.h"
#include "txdds/DCPS/common/InstanceHandle.h"

namespace BaoSky::dds
{
    struct PublicationMatchedStatus : public MatchedStatus
    {
        InstanceHandle last_subscription_handle;
    };
}

#endif
