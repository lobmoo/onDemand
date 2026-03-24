/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-07-01 13:38:51
 * @FilePath     : /TXDDS/include/txdds/DCPS/core/IDomainEntity.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */
#ifndef TXDDS_DCPS_IDOMAINENTITY_H
#define TXDDS_DCPS_IDOMAINENTITY_H

#include "txdds/DCPS/core/IEntity.h"

namespace BaoSky::dds
{
    class TXDDS_API IDomainEntity : public IEntity
    {
    public:
        IDomainEntity() : IEntity() {}

        virtual ~IDomainEntity() override {}
    };
}

#endif