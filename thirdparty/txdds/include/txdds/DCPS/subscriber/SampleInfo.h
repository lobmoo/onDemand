/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-09-20 09:35:53
 * @FilePath    : /TXDDS/include/DCPS/subscriber/SampleInfo.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_SAMPLEINFO_H
#define TXDDS_DCPS_SAMPLEINFO_H
#include <txdds/DCPS/common/InstanceHandle.h>
#include <txdds/RTPS/common/Time.h>
namespace BaoSky::dds
{
    enum SampleStateKind
    {
        READ,
        NOT_READ,
    };
    enum ViewStateKind
    {
        NEW,
        NOT_NEW
    };

    enum InstanceStateKind
    {
        ALIVE,
        NOT_ALIVE_DISPOSED,
        NOT_ALIVE_NO_WRITERS
    };

    struct SampleInfo
    {
        SampleStateKind sample_state;

        ViewStateKind view_state;

        InstanceStateKind instance_state;

        long disposed_generation_count;

        long no_writers_generation_count;

        long sample_rank;

        long generation_rank;

        long absolute_generation_rank;

        rtps::Time source_timestamp;

        rtps::Time reception_timestamp;

        InstanceHandle instance_handle;

        InstanceHandle publication_handle;
    };
}

#endif