/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-02-26 15:25:08
 * @FilePath     : /txcdr/include/DeserializeCdr.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXCDR_DESERIALIZECDR_H
#define TXCDR_DESERIALIZECDR_H

#include "txcdr/Cdr.h"
#include "txcdr/optional.h"
#include <functional>
#include <map>
#include <iostream>
namespace BaoSky::Cdr
{
    class DeserializeCdr;
}

template <class _T>
extern void DeserializeUserDefined(BaoSky::Cdr::DeserializeCdr &, _T &);
namespace BaoSky::Cdr
{
    class TXDDS_API DeserializeCdr : public Cdr
    {
    private:
        ReturnCode mLastStatus{RETCODE_OK};
        using DeserializeTypeFunctor = ReturnCode (DeserializeCdr::*)(EncodingAlgorithmFlag,
                                                                      std::function<bool(DeserializeCdr &, const uint32_t)>);
        DeserializeTypeFunctor mDeserializeType{nullptr};
        /* data */
    public:
        DeserializeCdr(TXBuffer &cdr_buffer, const Endianness endianness = DEFAULT_ENDIAN, const CdrVersion cdr_version = XCDRv2) : Cdr(cdr_buffer, endianness, cdr_version)
        {
            mLastStatus = RETCODE_OK;
            switch (mCdrVersion)
            {
            case CdrVersion::XCDRv2:
                mDeserializeType = &DeserializeCdr::DeserializeTypeXCDR2;
                break;
            case CdrVersion::XCDRv1:
                mDeserializeType = &DeserializeCdr::DeserializeTypeXCDR1;
                break;
            default:
                mDeserializeType = &DeserializeCdr::DeserializeTypeCDR;
            }
        }
        bool IsDeSerializeSuccess()
        {
            return mLastStatus == RETCODE_OK;
        }
        ~DeserializeCdr() {}
        ReturnCode Deserialize(uint8_t &octet_t);
        ReturnCode Deserialize(char &char_t);
        ReturnCode Deserialize(int8_t &int8);
        ReturnCode Deserialize(uint16_t &ushort_t);
        ReturnCode Deserialize(int16_t &short_t);
        ReturnCode Deserialize(uint32_t &ulong_t);
        ReturnCode Deserialize(int32_t &long_t);
        ReturnCode Deserialize(wchar_t &wchar);
        ReturnCode Deserialize(uint64_t &ulonglong_t);
        ReturnCode Deserialize(int64_t &longlong_t);
        ReturnCode Deserialize(float &float_t);
        ReturnCode Deserialize(double &double_t);
        ReturnCode Deserialize(bool &bool_t);
        ReturnCode Deserialize(char *&string_t);
        ReturnCode Deserialize(wchar_t *&string_t);
        ReturnCode Deserialize(std::string &string_t);
        ReturnCode Deserialize(std::wstring &string_t);
        template <class _T>
        ReturnCode DeserializeArray(_T *value, size_t num_elements)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }
            for (size_t count = 0; count < num_elements; ++count)
            {
                mLastStatus = Deserialize(value[count]);
                if (mLastStatus != RETCODE_OK)
                {
                    return mLastStatus;
                }
            }
            return mLastStatus;
        }

        template <class _T, typename std::enable_if<!std::is_enum<_T>::value>::type * = nullptr, typename = void>
        ReturnCode Deserialize(_T &value)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }
            DeserializeUserDefined(*this, value);
            return mLastStatus;
        }
        template <class _T>
        ReturnCode Deserialize(std::vector<_T> &value)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }

            uint32_t len = 0;
            mLastStatus = Deserialize(len);
            if (len >= MAX_SIZE || mLastStatus != RETCODE_OK)
            {
                mLastStatus = MAX_SIZE;
                return mLastStatus;
            }
            value.resize(len);
            mLastStatus = DeserializeArray(value.data(), len);
            return mLastStatus;
        }

        template <class _T, size_t _Size>
        ReturnCode Deserialize(
            std::array<_T, _Size> &array_t)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }
            mLastStatus = DeserializeArray(array_t.data(), array_t.size());
            return mLastStatus;
        }
        // Deserialize a map of non-primitive
        template <class _K, class _T, typename std::enable_if<!std::is_enum<_T>::value && !std::is_arithmetic<_T>::value>::type * = nullptr>
        ReturnCode Deserialize(std::map<_K, _T> &map_t)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }

            if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                // 适配当前版本序列化，注掉dheader相关代码
                // uint32_t dheader{0};
                // mLastStatus = Deserialize(dheader);
                // if (mLastStatus != 0)
                // {
                //     return mLastStatus;
                // }
                auto offset = mOffsetIter;
                uint32_t mapLength{0};
                mLastStatus = Deserialize(mapLength);
                if (mLastStatus != 0)
                {
                    return mLastStatus;
                }
                map_t.clear();
                uint32_t count{0};
                // while (mOffsetIter - offset < dheader && count < mapLength)
                while (count < mapLength)
                {
                    _K key;
                    _T val;
                    mLastStatus = Deserialize(key);
                    if (mLastStatus != 0)
                    {
                        return mLastStatus;
                    }
                    mLastStatus = Deserialize(val);
                    if (mLastStatus != 0)
                    {
                        return mLastStatus;
                    }
                    map_t.emplace(std::pair<_K, _T>(std::move(key), std::move(val)));
                    ++count;
                }
                // if (mOffsetIter - offset != dheader)
                // {
                //     mLastStatus = RETCODE_BAD_PARAM;
                //     return mLastStatus;
                // }
            }
            else
            {
                uint32_t sequenceLength = 0;
                mLastStatus = Deserialize(sequenceLength);
                if (mLastStatus != 0)
                {
                    return mLastStatus;
                }
                for (uint32_t i = 0; i < sequenceLength; ++i)
                {
                    _K key;
                    _T value;
                    mLastStatus = Deserialize(key);
                    if (mLastStatus != 0)
                    {
                        return mLastStatus;
                    }
                    mLastStatus = Deserialize(value);
                    if (mLastStatus != 0)
                    {
                        return mLastStatus;
                    }
                    map_t.emplace(std::pair<_K, _T>(std::move(key), std::move(value)));
                }
            }
            return mLastStatus;
        }

        // Deserialize a map of primitive
        template <class _K, class _T, typename std::enable_if<std::is_enum<_T>::value || std::is_arithmetic<_T>::value>::type * = nullptr>
        ReturnCode Deserialize(std::map<_K, _T> &map_t)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }
            uint32_t sequenceLength = 0;
            mLastStatus = Deserialize(sequenceLength);
            if (mLastStatus != 0)
            {
                return mLastStatus;
            }
            for (uint32_t i = 0; i < sequenceLength; ++i)
            {
                _K key;
                _T value;
                mLastStatus = Deserialize(key);
                if (mLastStatus != 0)
                {
                    return mLastStatus;
                }
                mLastStatus = Deserialize(value);
                if (mLastStatus != 0)
                {
                    return mLastStatus;
                }
                map_t.emplace(std::pair<_K, _T>(std::move(key), std::move(value)));
            }
            return mLastStatus;
        }

        template <class _T,
                  typename std::enable_if<std::is_enum<_T>::value>::type * = nullptr,
                  typename std::enable_if<std::is_same<typename std::underlying_type<_T>::type,
                                                       uint32_t>::value>::type * = nullptr>
        ReturnCode Deserialize(
            _T &value)
        {
            if (mLastStatus != RETCODE_OK)
            {
                return mLastStatus;
            }
            uint32_t decodeValue{0};
            mLastStatus = Deserialize(decodeValue);
            value = static_cast<_T>(decodeValue);
            return mLastStatus;
        }

        bool Xcdr1DeserializeMemberHeader(
            MemberId &member_id,
            Cdr::state &current_state);
        template <class _T>
        Cdr &Deserialize(
            optional<_T> &value)
        {
            // XCDR1
            if (EncodingAlgorithmFlag::PLAIN_CDR == mEncodingAlgorithmFlag)
            {
                Cdr::state current_state(*this);
                MemberId member_id;
                Xcdr1DeserializeMemberHeader(member_id, current_state);
                auto prev_offset = mOffsetIter;
                if (0 < current_state.member_size_)
                {
                    Deserialize(*value);
                }
                if (current_state.member_size_ != mOffsetIter - prev_offset)
                {
                    std::cout << "Member size provided by member header is not equal to the real decoded member size" << std::endl;
                }
            }
            else if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                bool is_present = true;
                // PLAIN_CDR2
                if (CdrVersion::XCDRv2 == mCdrVersion && EncodingAlgorithmFlag::PL_CDR2 != mEncodingAlgorithmFlag)
                {
                    Deserialize(is_present);
                }
                value.reset(is_present);
                if (is_present)
                {
                    Deserialize(*value);
                }
            }
            return *this;
        }
        ReturnCode DeserializeEncapsulation();
        // void CdrDeserializeType(std::function<bool(DeserializeCdr &)> functor);
        void CdrDeserializeTypeStart();
        void CdrDeserializeTypeEnd();

        ReturnCode DeserializeType(EncodingAlgorithmFlag typeEncoding, std::function<bool(DeserializeCdr &, const uint32_t)> functor);
        ReturnCode DeserializeTypeXCDR2(EncodingAlgorithmFlag typeEncoding, std::function<bool(DeserializeCdr &, const uint32_t)> functor);
        ReturnCode DeserializeTypeXCDR1(EncodingAlgorithmFlag typeEncoding, std::function<bool(DeserializeCdr &, const uint32_t)> functor);
        ReturnCode DeserializeTypeCDR(EncodingAlgorithmFlag typeEncoding, std::function<bool(DeserializeCdr &, const uint32_t)> functor);

        ReturnCode ReadString(char *&str, uint32_t &length);
        ReturnCode ReadWstring(std::wstring &wstr, uint32_t &length);
    };
}
#endif