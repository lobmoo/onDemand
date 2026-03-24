/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2025-01-04 14:32:59
 * @FilePath    : /TXDDS/include/DCPS/topic/TopicDataType.h
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_TOPICDATATYPE_H
#define TXDDS_DCPS_TOPICDATATYPE_H

#include "txdds/DCPS/common/InstanceHandle.h"
#include "txdds/RTPS/common/SerializedPayload.h"
#include "txdds/DCPS/dynamicType/DynamicTypeParameter.h"

#include <functional>

namespace BaoSky::dds
{
    class TXDDS_API TopicDataType
    {
        friend class TypeSupport;

    public:
        TopicDataType() : mTypeSize(0) {}

        virtual ~TopicDataType() {}

        virtual bool Serialize(void *data, BaoSky::rtps::SerializedPayload *payload) = 0;

        virtual bool Deserialize(BaoSky::rtps::SerializedPayload *payload, void *data) = 0;

        virtual std::function<uint32_t()> GetSerializedSizeProvider(void *data) = 0;

        virtual void *CreateData() = 0;

        virtual void DeleteData(void *data) = 0;

        virtual bool GetKey(void *data, BaoSky::rtps::InstanceHandle *ihandle, bool force_md5 = false) = 0;

        inline virtual void SetName(const char *nam)
        {
            mTypeName = std::string(nam);
        }

        inline virtual void SetTypeObjectFlag(bool flag)
        {
            mIsUseTypeObject = flag;
        }
        inline virtual void SetTypeInfomationFlag(bool flag)
        {
            mIsUseTypeInfomation = flag;
        }
        inline virtual const bool &GetTypeObjectFlag()
        {
            return mIsUseTypeObject;
        }
        inline virtual const bool &GetTypeInfomationFlag()
        {
            return mIsUseTypeInfomation;
        }

        uint32_t mTypeSize;
        std::string mTypeName;
        TypeIdentifierParameter mTypeIdentifierPara;
        TypeObjectParameter mTypeObjectPara;

    private:
        bool mIsUseTypeObject = false;
        bool mIsUseTypeInfomation = false;
    };
}

#endif