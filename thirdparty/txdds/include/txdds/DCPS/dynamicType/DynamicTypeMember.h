/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-03-13 15:59:03
 * @FilePath: /TXDDS/include/DCPS/dynamicType/DynamicTypeMember.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */

#pragma once
#include <string>
#include <vector>
#include "txdds/DCPS/dynamicType/CommonType.h"
#include "txdds/DCPS/dynamicType/MemberDescriptor.h"

namespace BaoSky::dds
{
    class TXDDS_API DynamicTypeMember
    {
    public:
        DynamicTypeMember(const MemberDescriptor &memberDescriptor);
        DynamicTypeMember(const DynamicTypeMember &) = delete;
        DynamicTypeMember &operator=(const DynamicTypeMember &) = delete;
        MemberDescriptor *GetMemberDescriptor();
        bool IsSameWithOtherMember(const DynamicTypeMember *other);
        std::string GetMemberName();
        MemberId GetMemberId();
        void SetMemberId(uint32_t id);
        void SetMemberIndex(uint32_t index);

    private:
        MemberDescriptor mMemberDescriptor;
    };
}
