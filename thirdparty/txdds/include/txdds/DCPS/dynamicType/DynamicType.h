/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-03-13 15:59:03
 * @FilePath: /TXDDS/include/DCPS/dynamicType/DynamicType.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */

#pragma once
#include "txdds/DCPS/dynamicType/DynamicTypeMember.h"
#include "txdds/DCPS/dynamicType/CommonType.h"
#include "txdds/DCPS/dynamicType/TypeDescriptor.h"

#include <string>
#include <map>

namespace BaoSky::dds
{
    class AnnotationDescriptor;
    class DynamicTypeBuilder;
    class TXDDS_API DynamicType
    {
    public:
        DynamicType(const TypeDescriptor &typeDescriptor);
        bool Equals(DynamicType *other);
        AnnotationDescriptor *GetAnnotation(uint32_t index);
        uint32_t GetAnnotationCount();
        TypeDescriptor &GetDescriptor();
        TypeKind GetKind();
        std::shared_ptr<DynamicTypeMember> GetMemberById(MemberId memberId);
        std::shared_ptr<DynamicTypeMember> GetMemberByIndex(uint32_t index);
        std::shared_ptr<DynamicTypeMember> GetMemberByName(std::string name);
        uint32_t GetMemberCount();
        std::string GetName();
        ReturnCode CopyFrom(const DynamicTypeBuilder *builder);
        const std::map<MemberId, std::shared_ptr<DynamicTypeMember>> &GetAllMembersById();

    private:
        std::map<std::string, std::shared_ptr<DynamicTypeMember>> mMemberByName;
        std::map<MemberId, std::shared_ptr<DynamicTypeMember>> mMemberById;
        TypeDescriptor mTypeDescriptor;
    };
}
