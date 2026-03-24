/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-04-06 18:21:21
 * @FilePath: /TXDDS/include/DCPS/dynamicType/DynamicTopicDataType.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */
#pragma once
#include <map>

#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/dynamicType/DynamicType.h"
#include "txdds/DCPS/topic/TopicDataType.h"
using ReturnCode = BaoSky::dds::ReturnCode;
namespace BaoSky::dds
{
    class TXDDS_API DynamicTopicDataType : public virtual TopicDataType
    {
    public:
        DynamicTopicDataType(std::shared_ptr<DynamicType> dynamicType);
        ~DynamicTopicDataType();
        DynamicTopicDataType(const DynamicTopicDataType &) = delete;
        DynamicTopicDataType &operator=(const DynamicTopicDataType &) = delete;
        /**
         * @brief 序列化DynamicData
         * @param  *data {void}
         * @param  *payload {SerializedPayload}
         * @return {bool}
         */
        virtual bool Serialize(void *data, BaoSky::rtps::SerializedPayload *payload) override;
        /**
         * @brief  反序列化DynamicData
         * @param  *payload {SerializedPayload}
         * @param  *data {void}
         * @return {}
         */
        virtual bool Deserialize(BaoSky::rtps::SerializedPayload *payload, void *data) override;
        /**
         * @brief 创建DynamicData指针
         * @return {}
         */
        virtual void *CreateData() override;
        /**
         * @brief 删除DynamicData指针
         * @param  *data {void}
         * @return {}
         */
        virtual void DeleteData(void *data) override;
        /**
         * @brief  获取DynamicData序列化的大小
         * @param  *data {void}
         * @return {}
         */
        virtual std::function<uint32_t()> GetSerializedSizeProvider(void *data) override;
        /**
         * @brief  获取key
         * @param  *data {void}
         * @param  *ihandle {InstanceHandle}
         * @param  force_md5 {bool}
         * @return {}
         */
        virtual bool GetKey(void *data, BaoSky::rtps::InstanceHandle *ihandle, bool force_md5 = false) override;
        /**
         * @brief  注册TypeObject类型
         * @param  &typeObjectParameter {TypeObjectParameter}
         * @param  &typeIdentifierParameter {TypeIdentifierParameter}
         * @return {}
         */
        virtual ReturnCode RegisterTypeObject(TypeObjectParameter &typeObjectParameter, TypeIdentifierParameter &typeIdentifierParameter);

    private:
        /**
         * @brief  计算DynamicType序列化的大小
         * @return {uint32_t}
         */
        virtual uint32_t CaculateDynamicTopicDataTypeSize();
        // DynamicData对应的DynamicType结构
        std::shared_ptr<DynamicType> mDynamicType;
    };
}