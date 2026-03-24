/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 17:34:20
 * @FilePath: /TXDDS/include/RTPS/attributes/TopicAttributes.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_TopicAttributes_H
#define TXDDS_RTPS_TopicAttributes_H
#include "txdds/DCPS/core/policy/QosPolicy.h"
#include "txdds/RTPS/common/RTPSEntityTypes.h"
#include "txdds/DCPS/dynamicType/DynamicTypeParameter.h"
#include <string>

namespace BaoSky::rtps
{
    class ContentFilterProperty_t
    {
    public:
        ContentFilterProperty_t() = default;
        virtual ~ContentFilterProperty_t() = default;
    };
    class TopicAttributes
    {
    public:
        TopicAttributes() = default;
        virtual ~TopicAttributes() = default;
        std::string mTopicName;
        std::string mTopicDataType;
        TopicKind_t mTopicKind = TopicKind_t::NO_KEY;
        BaoSky::dds::HistoryQosPolicy mHistoryQosPolicy;
        bool mIsUseTypeObject = false;
        bool mIsUseTypeInfomation = false;
        // todo:dynamic type
        BaoSky::dds::TypeIdentifierParameter mTypeIdentifierPara;
        BaoSky::dds::TypeObjectParameter mTypeObjectPara;
    };
}

#endif