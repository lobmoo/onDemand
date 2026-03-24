/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-03-13 15:59:03
 * @FilePath: /TXDDS/include/DCPS/dynamicType/DynamicTypeBuilder.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */

#pragma once
#include "txdds/DCPS/dynamicType/DynamicType.h"
#include <map>

namespace BaoSky::dds
{
    class AnnotationDescriptor;
    class TXDDS_API DynamicTypeBuilder
    {
        friend DynamicType;

    public:
        DynamicTypeBuilder(const TypeDescriptor &typeDescriptor);
        ReturnCode AddMember(MemberDescriptor *descriptor);
        ReturnCode ApplyAnnotation(AnnotationDescriptor *descriptor);
        ReturnCode ApplyAnnotationToMember(MemberId memberId, AnnotationDescriptor *descriptor);
        std::shared_ptr<DynamicType> Build();
        bool Equals(DynamicType *other);
        ReturnCode GetAnnotation(AnnotationDescriptor *descriptor, uint32_t index);
        uint32_t GetAnnotationCount();
        TypeDescriptor &GetDescriptor();
        TypeKind GetKind();
        std::shared_ptr<DynamicTypeMember> GetMemberById(MemberId memberId);
        std::shared_ptr<DynamicTypeMember> GetMemberByIndex(uint32_t index);
        std::shared_ptr<DynamicTypeMember> GetMemberByName(std::string name);
        uint32_t GetMemberCount();
        std::string GetName();
        void SetName(const std::string &name)
        {
            mTypeDescriptor.SetDynTypeName(name);
        }

    private:
        std::map<std::string, std::shared_ptr<DynamicTypeMember>> mMemberByName;
        std::map<MemberId, std::shared_ptr<DynamicTypeMember>> mMemberById;
        TypeDescriptor mTypeDescriptor;
        uint32_t mCurrentIndex{0};
        std::vector<AnnotationDescriptor *> mAnnotationDescriptors;
    };
}