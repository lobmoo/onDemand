/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-03-13 15:59:03
 * @FilePath: /TXDDS/include/DCPS/dynamicType/DynamicTypeBuilderFactory.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */

#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <memory>

#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/dynamicType/CommonType.h"
#include "txdds/DCPS/dynamicType/TypeDescriptor.h"
#include "txdds/DCPS/dynamicType/DynamicTypeBuilder.h"
#include "txdds/DCPS/dynamicType/DynamicType.h"
using ReturnCode = BaoSky::dds::ReturnCode;

namespace BaoSky::dds
{
    class TypeObject;
    class TXDDS_API DynamicTypeBuilderFactory
    {
    public:
        DynamicTypeBuilderFactory(const DynamicTypeBuilderFactory &) = delete;
        DynamicTypeBuilderFactory &operator=(const DynamicTypeBuilderFactory &) = delete;
        static DynamicTypeBuilderFactory &GetInstance();
        static ReturnCode DeleteInstance();
        const std::shared_ptr<DynamicType> &GetPrimitiveType(TypeKind kind);
        DynamicTypeBuilder *CreateTypeBuilder(TypeDescriptor *descriptor);
        DynamicTypeBuilder *CreateTypeBuilderCopy(std::shared_ptr<DynamicType> dynamicType);
        DynamicTypeBuilder *CreateTypeBuilderByTypeObject(TypeObject *typeObject);
        DynamicTypeBuilder *CreateStringTypeBuilder(uint32_t bound);
        DynamicTypeBuilder *CreateWstringTypeBuilder(uint32_t bound);
        DynamicTypeBuilder *CreateSequenceTypeBuilder(uint32_t bound, std::shared_ptr<DynamicType> elementType);
        DynamicTypeBuilder *CreateArrayTypeBuilder(std::vector<uint32_t> bound, std::shared_ptr<DynamicType> elementType);
        DynamicTypeBuilder *CreateMapTypeBuilder(uint32_t bound, std::shared_ptr<DynamicType> keyElementType, std::shared_ptr<DynamicType> valueElementType);
        DynamicTypeBuilder *CreateBitmaskTypeBuilder(uint32_t bound);
        DynamicTypeBuilder *CreateTypeBuilderByUri(std::string documentUri, std::string typeName, std::string includePaths);
        DynamicTypeBuilder *CreateTypeBuilderByDocument(std::string document, std::string typeName, std::string includePaths);
        ReturnCode DeleteType(DynamicType *dynType);
        ReturnCode DeleteTypeBuilder(DynamicTypeBuilder *dynTypeBuilder);
        void AddBuilderTolist(DynamicTypeBuilder *pBuilder);
        DynamicTypeBuilder *CreateCustomBuilder(const TypeDescriptor *descriptor, const std::string &name = "");

    private:
        DynamicTypeBuilderFactory();
        ~DynamicTypeBuilderFactory();
        std::shared_ptr<DynamicType> mBoolDynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_BOOLEAN))};
        std::shared_ptr<DynamicType> mByteDynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_BYTE))};
        std::shared_ptr<DynamicType> mInt8DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_INT8))};
        std::shared_ptr<DynamicType> mUint8DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_UINT8))};
        std::shared_ptr<DynamicType> mInt16DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_INT16))};
        std::shared_ptr<DynamicType> mUint16DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_UINT16))};
        std::shared_ptr<DynamicType> mInt32DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_INT32))};
        std::shared_ptr<DynamicType> mUint32DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_UINT32))};
        std::shared_ptr<DynamicType> mInt64DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_INT64))};
        std::shared_ptr<DynamicType> mUint64DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_UINT64))};
        std::shared_ptr<DynamicType> mFloat32DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_FLOAT32))};
        std::shared_ptr<DynamicType> mFloat64DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_FLOAT64))};
        std::shared_ptr<DynamicType> mFloat128DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_FLOAT128))};
        std::shared_ptr<DynamicType> mChar8DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_CHAR8))};
        std::shared_ptr<DynamicType> mChar16DynamicType{std::make_shared<DynamicType>(TypeDescriptor(TypeKind_CHAR16))};
        std::shared_ptr<DynamicType> mErrorDynamicType{std::shared_ptr<DynamicType>(nullptr)};
        std::vector<DynamicTypeBuilder *> mBuildLists;
    };
}
