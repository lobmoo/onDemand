/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-03-25 17:19:43
 * @FilePath: /TXDDS/include/DCPS/dynamicType/MemberDescriptor.h
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

    class TXDDS_API MemberDescriptor
    {
    public:
        MemberDescriptor();
        MemberDescriptor(const MemberDescriptor &) = delete;
        MemberDescriptor &operator=(const MemberDescriptor &) = delete;
        ReturnCode CopyFromOtherMemberDescriptor(const MemberDescriptor *other);
        bool IsSameWithOtherMemberDescriptor(const MemberDescriptor *other);
        bool IsConsistent();
        const std::string &GetName();
        MemberId GetMemberId();
        void SetMemberId(uint32_t id);
        void SetIndex(uint32_t index);
        std::shared_ptr<DynamicType> GetType();
        void SetMemberName(std::string name);
        void SetMemberType(std::shared_ptr<DynamicType> type);

    private:
        std::string mName;
        MemberId mId;
        std::shared_ptr<DynamicType> mType;
        std::string mDefaultValue;
        uint32_t mIndex;
        std::vector<int64_t> mLabel;
        bool mDefaultLabel;
    };
}