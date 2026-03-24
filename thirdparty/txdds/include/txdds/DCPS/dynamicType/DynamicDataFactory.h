/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-04-02 10:05:58
 * @FilePath: /TXDDS/include/DCPS/dynamicType/DynamicDataFactory.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */
#pragma once
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/dynamicType/DynamicType.h"
#include "txdds/DCPS/dynamicType/DynamicData.h"
using ReturnCode = BaoSky::dds::ReturnCode;

namespace BaoSky::dds
{
    class DynamicData;
    class TXDDS_API DynamicDataFactory
    {
    public:
        DynamicDataFactory(const DynamicDataFactory &) = delete;
        DynamicDataFactory &operator=(const DynamicDataFactory &) = delete;
        static DynamicDataFactory &GetInstance();
        DynamicData *CreateData(std::shared_ptr<DynamicType> dynamicType);
        ReturnCode DeleteData(DynamicData *dynamicData);

    private:
        DynamicDataFactory() = default;
        ~DynamicDataFactory() = default;
    };
}