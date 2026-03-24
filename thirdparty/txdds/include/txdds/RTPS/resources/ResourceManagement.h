/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-03-07 14:04:30
 * @FilePath     : /TXDDS/include/RTPS/resources/ResourceManagement.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_RESOURCE_MANAGEMENT_H
#define TXDDS_RTPS_RESOURCE_MANAGEMENT_H

namespace BaoSky::rtps
{
    typedef enum MemoryManagementPolicy
    {
        PREALLOCATED_MEMORY_MODE,
        PREALLOCATED_WITH_REALLOC_MEMORY_MODE,
        DYNAMIC_RESERVE_MEMORY_MODE,
        DYNAMIC_REUSABLE_MEMORY_MODE
    } MemoryManagementPolicy_t;

}

#endif