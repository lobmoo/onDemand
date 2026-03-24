/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-03-13 15:59:03
 * @FilePath: /TXDDS/include/DCPS/dynamicType/TypeDescriptor.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "txdds/DCPS/dynamicType/CommonType.h"
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/txddsexport.h"
using ReturnCode = BaoSky::dds::ReturnCode;

namespace BaoSky::dds
{
    class DynamicType;

    class TXDDS_API TypeDescriptor
    {
    public:
        TypeDescriptor();
        TypeDescriptor(TypeKind kind);
        TypeDescriptor(const TypeDescriptor &) = delete;
        TypeDescriptor &operator=(const TypeDescriptor &) = delete;

        // set member attribute
        /**
         * @brief 设置TypeDescriptor基础类型
         * @param  dynamicType {shared_ptr<DynamicType>}
         * @return {}
         */
        void SetDynTypeBaseType(std::shared_ptr<DynamicType> dynamicType);
        /**
         * @brief  设置动态类型TypeDescriptor的键类型(map等)
         * @param  dynamicType {shared_ptr<DynamicType>}
         * @return {}
         */
        void SetDynTypeKeyType(std::shared_ptr<DynamicType> dynamicType);
        /**
         * @brief  设置动态类型种类
         * @param  kind {TypeKind}
         * @return {}
         */
        void SetDynTypeKind(TypeKind kind);
        /**
         * @brief 设置动态类型名字
         * @param  name {string}
         * @return {}
         */
        void SetDynTypeName(std::string name);
        /**
         * @brief  设置动态类型TypeDescriptor的元素类型(array、sequence等)
         * @param  dynamicType {shared_ptr<DynamicType>}
         * @return {}
         */
        void SetDynTypeElementType(std::shared_ptr<DynamicType> dynamicType);
        // get member attribute
        /**
         * @brief  获取动态类型TypeDescriptor的元素类型(array、sequence等)
         * @return {std::shared_ptr<DynamicType>}
         */
        std::shared_ptr<DynamicType> GetDynTypeElementType();
        /**
         * @brief 获取动态类型TypeDescriptor的键类型(map等)
         * @return {std::shared_ptr<DynamicType>}
         */
        std::shared_ptr<DynamicType> GetDynTypeKeyType();
        /**
         * @brief 获取动态类型名字
         * @return {std::string}
         */
        std::string GetDynTypeName();
        /**
         * @brief 获取动态类型边界(array、sequence等)
         * @return {std::vector<uint32_t>&}
         */
        std::vector<uint32_t> &GetDynTypeBound();
        /**
         * @brief 获取动态类型种类
         * @return {TypeKind}
         */
        TypeKind GetDynTypeKind() const;
        /**
         * @brief 比较两个TypeDescriptor是否一致
         * @param  *other {TypeDescriptor}
         * @return {bool}
         */
        bool IsSameWithOtherType(const TypeDescriptor *other);
        /**
         * @brief 检查动态类型一致性
         * @return {bool}
         */
        bool IsTypeConsistent();
        /**
         * @brief  从另一个TypeDescriptor拷贝
         * @param  *other {TypeDescriptor}
         * @return {ReturnCode}
         */
        ReturnCode CopyFromOtherType(const TypeDescriptor *other);

    private:
        TypeKind mDynTypeKind;
        std::string mDynTypeName;
        std::shared_ptr<DynamicType> mDynTypeBaseType;
        std::vector<uint32_t> mDynTypeBound;
        std::shared_ptr<DynamicType> mDynTypeElementType;
        std::shared_ptr<DynamicType> mDynTypeKeyType;
    };
}