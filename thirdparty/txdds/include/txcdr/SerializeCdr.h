/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-02-26 15:24:43
 * @FilePath     : /txcdr/include/SerializeCdr.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXCDR_SERIALIZECDR_H
#define TXCDR_SERIALIZECDR_H

#include "txcdr/Cdr.h"
#include "txcdr/optional.h"
#include <map>
namespace BaoSky::Cdr
{
    class SerializeCdr;
}

template <class _T>
extern void SerializeUserDefined(BaoSky::Cdr::SerializeCdr &, const _T &);
namespace BaoSky::Cdr
{

    class TXDDS_API SerializeCdr : public Cdr
    {
    private:
    public:
        bool IsSuccessSerialize()
        {
            return mLastStatus == RETCODE_OK;
        }
        SerializeCdr(TXBuffer &cdr_buffer, const Endianness endianness = DEFAULT_ENDIAN, const CdrVersion cdr_version = XCDRv2) : Cdr(cdr_buffer, endianness, cdr_version) {}
        ~SerializeCdr() {}
        ReturnCode Serialize(const uint8_t &octet_t);
        ReturnCode Serialize(const char char_t);
        ReturnCode Serialize(const int8_t int8);
        ReturnCode Serialize(const uint16_t ushort_t);
        ReturnCode Serialize(const int16_t short_t);
        ReturnCode Serialize(const uint32_t ulong_t);
        ReturnCode Serialize(const int32_t long_t);
        ReturnCode Serialize(const wchar_t wchar);
        ReturnCode Serialize(const uint64_t ulonglong_t);
        ReturnCode Serialize(const int64_t longlong_t);
        ReturnCode Serialize(const float float_t);
        ReturnCode Serialize(const double double_t);
        ReturnCode Serialize(const bool bool_t);
        ReturnCode Serialize(char *string_t);
        ReturnCode Serialize(const char *string_t);
        ReturnCode Serialize(const wchar_t *string_t);
        ReturnCode Serialize(const std::string &string_t);
        ReturnCode Serialize(const std::wstring &string_t);
        template <class _T>
        ReturnCode SerializeArray(const _T *value, size_t numElements)
        {
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            for (size_t count = 0; count < numElements; ++count)
            {
                mLastStatus = Serialize(value[count]);
                if (mLastStatus != 0)
                {
                    return mLastStatus;
                }
            }
            return mLastStatus;
        }
        template <class _T, typename std::enable_if<!std::is_enum<_T>::value>::type * = nullptr, typename = void>
        ReturnCode Serialize(const _T &value)
        {
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            SerializeUserDefined(*this, value);
            return mLastStatus;
        }

        template <class _T>
        ReturnCode Serialize(const std::vector<_T> &vector_t)
        {
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            if (vector_t.size() >= MAX_SIZE)
            {
                mLastStatus = RETCODE_EXCEED_MAX_SIZE;
                return mLastStatus;
            }
            mLastStatus = Serialize(static_cast<int32_t>(vector_t.size()));
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            mLastStatus = SerializeArray(vector_t.data(), vector_t.size());
            return mLastStatus;
        }

        template <class _K, class _T>
        ReturnCode Serialize(const std::map<_K, _T> &map_t)
        {
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            mLastStatus = Serialize(static_cast<int32_t>(map_t.size()));
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            for (auto it_pair = map_t.begin(); it_pair != map_t.end(); ++it_pair)
            {
                mLastStatus = Serialize(it_pair->first);
                if (mLastStatus != 0)
                {
                    return mLastStatus;
                }
                mLastStatus = Serialize(it_pair->second);
                if (mLastStatus != 0)
                {
                    return mLastStatus;
                }
            }
            return mLastStatus;
        }

        template <class _T, size_t _Size>
        ReturnCode Serialize(
            const std::array<_T, _Size> &array_t)
        {
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            mLastStatus = SerializeArray(array_t.data(), array_t.size());
            return mLastStatus;
        }

        template <class _T,
                  typename std::enable_if<std::is_enum<_T>::value>::type * = nullptr,
                  typename std::enable_if<std::is_same<typename std::underlying_type<_T>::type,
                                                       uint32_t>::value>::type * = nullptr>
        ReturnCode Serialize(
            const _T &value)
        {
            mLastStatus = Serialize(static_cast<uint32_t>(value));
            return mLastStatus;
        }

        ReturnCode Serialize(const MemberId &value)
        {
            this->mNextMemberId = value;
            return mLastStatus;
        }

        template <class _T>
        ReturnCode Serialize(
            const optional<_T> &value)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }
            // MemberId 需要定义或者传入
            // Added by tangfuqi@baosight.com on 2026-03-11 15:08:24
            if (CdrVersion::XCDRv1 == mCdrVersion)
            {
                // PLAIN_CDR
                Cdr::state current_state(*this);
                mLastStatus = Xcdr1BeginSerializeOptionalMember(mNextMemberId, value.has_value(), current_state, XCdrHeaderSelection::AUTO_WITH_SHORT_HEADER_BY_DEFAULT);
                if (mLastStatus != RETCODE_OK)
                {
                    return mLastStatus;
                }
                if (value.has_value())
                {
                    mLastStatus = Serialize(*value);
                    if (mLastStatus != RETCODE_OK)
                    {
                        return mLastStatus;
                    }
                }
                mLastStatus = Xcdr1EndSerializeOptionalMember(current_state);
            }
            else if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                // PLAIN_CDR2
                if (EncodingAlgorithmFlag::PL_CDR2 != mEncodingAlgorithmFlag)
                {
                    mLastStatus = Serialize(value.has_value());
                    if (mLastStatus != RETCODE_OK)
                    {
                        return mLastStatus;
                    }
                }
                if (value.has_value())
                {
                    mLastStatus = Serialize(*value);
                }
            }
            return mLastStatus;
        }
        ReturnCode Xcdr1BeginSerializeOptionalMember(
            const MemberId &member_id,
            bool is_present,
            Cdr::state &current_state,
            Cdr::XCdrHeaderSelection header_selection);
        ReturnCode Xcdr1EndSerializeOptionalMember(
            const Cdr::state &current_state);

        ReturnCode Xcdr1SerializeShortMemberHeader(
            const MemberId &member_id);

        ReturnCode Xcdr1EndShortMemberHeader(
            const MemberId &member_id,
            size_t member_serialized_size);

        ReturnCode SerializeEncapsulation();

        ReturnCode CdrBeginSerializeType();

        ReturnCode CdrEndSerializeType();

        // TODO enum map longdouble
    };
}
#endif