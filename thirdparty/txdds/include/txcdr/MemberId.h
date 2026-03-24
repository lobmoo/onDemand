/*
 * @Author       : tangfuqi tangfuqi@baosight.com
 * @Date         : 2026-03-12 09:39:02
 * @FilePath     : /TXDDS/thirdparty/txcdr/include/txcdr/MemberId.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#pragma once
#include <stdint.h>
#include "txdds/txddsexport.h"
namespace BaoSky::Cdr
{

    class TXDDS_API MemberId
    {
    public:
        MemberId() = default;

        MemberId(
            uint32_t id_value)
            : id(id_value)
        {
        }

        bool operator==(
            uint32_t id_value) const
        {
            return id == id_value;
        }

        bool operator==(
            const MemberId member_id) const
        {
            return id == member_id.id;
        }

        bool operator!=(
            const MemberId member_id) const
        {
            return !(member_id == *this);
        }

        uint32_t id{member_id_invalid_value_};

        bool must_understand{false};

    private:
        static constexpr uint32_t member_id_invalid_value_ = 0xFFFFFFFF;
    };
    static const MemberId MEMBER_ID_INVALID{};
    const uint32_t MAX_SIZE = 100 * 1024 * 1024;

}