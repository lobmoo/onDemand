/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 17:59:26
 * @FilePath: /TXDDS/include/RTPS/builtin/data/DiscoveredReaderData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_DiscoveredReaderData_H
#define TXDDS_RTPS_DiscoveredReaderData_H

#include "txdds/DCPS/builtin/topic/SubscriptionBuiltinTopicData.h"
#include "txdds/RTPS/builtin/data/ReaderProxyData.h"
#include "txdds/RTPS/attributes/TopicAttributes.h"
#include "txdds/RTPS/common/CDRMessage.h"
#include "txdds/DCPS/dynamicType/DynamicTypeParameter.h"

using SubscriptionBuiltinTopicData = BaoSky::dds::builtin::SubscriptionBuiltinTopicData;
using TypeIdentifierParameter = BaoSky::dds::TypeIdentifierParameter;
using TypeObjectParameter = BaoSky::dds::TypeObjectParameter;
namespace BaoSky::rtps
{
    class TXDDS_API DiscoveredReaderData
    {
    public:
        DiscoveredReaderData() = default;
        virtual ~DiscoveredReaderData() = default;
        bool WriteCDRMessage(CDRMessage &msg, bool isWriteEncapsulation);
        bool ReadFromCDRMessage(CDRMessage *msg, bool isUseShmTransport, bool isFilterLocator, VendorId_t sourceVendor = VENDORID_TXDDS);

        inline std::string GetTopicName() 
        {
            return mReaderProxyData.GetTopicName();
        }
        inline void SetKey(const BaoSky::rtps::Guid &key)
        {
            this->mInstanceHandle = key;
            this->mReaderProxyData.SetKey(key);
        }
        inline void SetTypeIdentifierParameter(const TypeIdentifierParameter &typeIdentifierParameter)
        {
            mTypeIdentifierPara = typeIdentifierParameter;
        }
        inline void SetTypeObjectParameter(const TypeObjectParameter &typeObjectParameter)
        {
            mTypeObjectPara = typeObjectParameter;
        }
        inline std::string GetTypeName()
        {
            return mReaderProxyData.GetTypeName();
        }
        BaoSky::rtps::Guid &GetReaderGuid();

        SubscriptionBuiltinTopicData mDDSSubscriptionData;
        ReaderProxyData mReaderProxyData;
        ContentFilterProperty_t mContentFilter;
        InstanceHandle mInstanceHandle;
        TypeIdentifierParameter mTypeIdentifierPara;
        TypeObjectParameter mTypeObjectPara;
        bool mIsUseTypeObject = false;
        bool mIsUseTypeInfomation = false;
    };
}

#endif