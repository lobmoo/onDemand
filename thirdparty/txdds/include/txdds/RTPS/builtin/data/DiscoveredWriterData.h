/*
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2024-07-19 17:59:58
 * @FilePath: /TXDDS/include/RTPS/builtin/data/DiscoveredWriterData.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_DiscoveredWriterData_H
#define TXDDS_RTPS_DiscoveredWriterData_H

#include "txdds/DCPS/builtin/topic/PublicationBuiltinTopicData.h"
#include "txdds/RTPS/builtin/data/WriterProxyData.h"
#include "txdds/RTPS/common/CDRMessage.h"
#include "txdds/DCPS/dynamicType/DynamicTypeParameter.h"
using PublicationBuiltinTopicData = BaoSky::dds::builtin::PublicationBuiltinTopicData;
using TypeIdentifierParameter = BaoSky::dds::TypeIdentifierParameter;
using TypeObjectParameter = BaoSky::dds::TypeObjectParameter;
namespace BaoSky::rtps
{
    class TXDDS_API DiscoveredWriterData
    {
    public:
        DiscoveredWriterData() = default;
        virtual ~DiscoveredWriterData() = default;
        bool WriteCDRMessage(CDRMessage &msg, bool isWriteEncapsulation);
        bool ReadFromCDRMessage(CDRMessage *msg, bool isUseShmTransport, bool isFilterLocator, VendorId_t sourceVendor = VENDORID_TXDDS);

        inline std::string GetTopicName() const
        {
            return mWriterProxyData.GetTopicName();
        }
        inline void SetKey(const BaoSky::rtps::Guid &key)
        {
            this->mInstanceHandle = key;
            this->mWriterProxyData.SetKey(key);
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
            return mWriterProxyData.GetTypeName();
        }
        BaoSky::rtps::Guid &GetWriterGuid();
        PublicationBuiltinTopicData mDDSPublicationData;
        WriterProxyData mWriterProxyData;
        InstanceHandle mInstanceHandle;
        TypeIdentifierParameter mTypeIdentifierPara;
        TypeObjectParameter mTypeObjectPara;
        bool mIsUseTypeObject = false;
        bool mIsUseTypeInfomation = false;
    };
}

#endif