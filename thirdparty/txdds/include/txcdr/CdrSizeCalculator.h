/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-02-17 15:22:52
 * @FilePath     : /TXDDS/thirdparty/txcdr/include/txcdr/CdrSizeCalculator.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXCDR_CDRSIZECALCULATOR_H
#define TXCDR_CDRSIZECALCULATOR_H

#include <vector>
#include <array>
#include <map>
#include <cstdint>
#include <string>

#include "txdds/txddsexport.h"
#include "txcdr/Enums.h"
#include "txcdr/optional.h"
#include "txcdr/MemberId.h"
#include <limits>
namespace BaoSky::Cdr
{
    class CdrSizeCalculator;
}
template <class _T>
extern std::size_t CalculateSerializedSizeUserDefined(
    BaoSky::Cdr::CdrSizeCalculator &,
    const _T &,
    std::uint64_t &);

template <class _T>
extern std::size_t CalculateSerializedSizeUserDefined(
    BaoSky::Cdr::CdrSizeCalculator &,
    const _T &,
    uint32_t);
namespace BaoSky::Cdr
{

    class TXDDS_API CdrSizeCalculator
    {
    public:
        CdrSizeCalculator(CdrVersion version);
        /*!
         * @brief Constructor.
         * @param[in] cdr_version Represents the version of the encoding algorithm that will be used for the encoding.
         * The default value is CdrVersion::XCDRv2.
         * @param[in] encoding Represents the initial encoding.
         */
        CdrSizeCalculator(
            CdrVersion cdr_version,
            EncodingAlgorithmFlag encoding);

        /*!
         * @brief Retrieves the version of the encoding algorithm used by the instance.
         * @return Configured CdrVersion.
         */
        CdrVersion GetCdrVersion() const;

        /*!
         * @brief Retrieves the current encoding algorithm used by the instance.
         * @return Configured EncodingAlgorithmFlag.
         */
        EncodingAlgorithmFlag GetEncoding() const;

        /*!
         * @brief Generic template which calculates the encoded size of an instance of an unknown type.
         * @tparam _T Instance's type.
         * @param[in] data Reference to the instance.
         * @param[inout] current_alignment Current Alignment in the encoding.
         * @return Encoded size of the instance.
         */
        template <class _T, typename std::enable_if<!std::is_enum<_T>::value>::type * = nullptr, typename = void>
        std::size_t CalculateSerializedSize(const _T &data, std::size_t &current_alignment)
        {
            uint64_t a = current_alignment;
            auto value = CalculateSerializedSizeUserDefined(*this, data, a);
            current_alignment = a;
            return value;
        }

        template <class _T,
                  typename std::enable_if<std::is_enum<_T>::value>::type * = nullptr,
                  typename std::enable_if<std::is_same<typename std::underlying_type<_T>::type,
                                                       uint32_t>::value>::type * = nullptr>
        size_t CalculateSerializedSize(
            const _T &data,
            size_t &current_alignment)
        {
            return CalculateSerializedSize(static_cast<uint32_t>(data), current_alignment);
        }

        std::size_t CalculateSerializedSize(const int8_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            ++current_alignment;
            return 1;
        }

        std::size_t CalculateSerializedSize(const uint8_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            ++current_alignment;
            return 1;
        }

        std::size_t CalculateSerializedSize(const char &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            ++current_alignment;
            return 1;
        }

        std::size_t CalculateSerializedSize(const bool &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            ++current_alignment;
            return 1;
        }

        std::size_t CalculateSerializedSize(const wchar_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{2 + Alignment(current_alignment, 2)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const int16_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{2 + Alignment(current_alignment, 2)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const uint16_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{2 + Alignment(current_alignment, 2)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const int32_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{4 + Alignment(current_alignment, 4)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const uint32_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{4 + Alignment(current_alignment, 4)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const int64_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{8 + Alignment(current_alignment, align64_)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const uint64_t &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{8 + Alignment(current_alignment, align64_)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const float &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{4 + Alignment(current_alignment, 4)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const double &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{8 + Alignment(current_alignment, align64_)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const long double &data, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{16 + Alignment(current_alignment, align64_)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const std::string &data, std::size_t &current_alignment)
        {
            std::size_t calculated_size{4 + Alignment(current_alignment, 4) + data.size() + 1};
            current_alignment += calculated_size;
            serialized_member_size_ = SERIALIZED_MEMBER_SIZE;

            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const std::wstring &data, std::size_t &current_alignment)
        {
            std::size_t calculated_size{4 + Alignment(current_alignment, 4) + data.size() * 2};
            current_alignment += calculated_size;

            return calculated_size;
        }

        template <class _T, typename std::enable_if<!std::is_enum<_T>::value &&
                                                    !std::is_arithmetic<_T>::value>::type * = nullptr>
        std::size_t CalculateSerializedSize(const std::vector<_T> &data, std::size_t &current_alignment)
        {
            std::size_t initial_alignment{current_alignment};

            if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                // DHEADER
                current_alignment += 4 + Alignment(current_alignment, 4);
            }

            current_alignment += 4 + Alignment(current_alignment, 4);

            std::size_t calculated_size{current_alignment - initial_alignment};
            calculated_size += CalculateArraySerializedSize(data.data(), data.size(), current_alignment);

            if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                // Inform DHEADER can be joined with NEXTINT
                serialized_member_size_ = SERIALIZED_MEMBER_SIZE;
            }

            return calculated_size;
        }

        /*!
         * @brief Specific template which calculates the encoded size of an instance of a sequence of primitives.
         * @param[in] data Reference to the instance.
         * @param[inout] current_alignment Current Alignment in the encoding.
         * @return Encoded size of the instance.
         */
        template <class _T, typename std::enable_if<std::is_enum<_T>::value ||
                                                    std::is_arithmetic<_T>::value>::type * = nullptr>
        std::size_t CalculateSerializedSize(const std::vector<_T> &data, std::size_t &current_alignment)
        {
            std::size_t initial_alignment{current_alignment};

            current_alignment += 4 + Alignment(current_alignment, 4);

            std::size_t calculated_size{current_alignment - initial_alignment};
            calculated_size += CalculateArraySerializedSize(data.data(), data.size(), current_alignment);

            if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                serialized_member_size_ = get_serialized_member_size<_T>();
            }

            return calculated_size;
        }

        std::size_t CalculateSerializedSize(const std::vector<bool> &data, std::size_t &current_alignment)
        {
            std::size_t calculated_size{data.size() + 4 + Alignment(current_alignment, 4)};
            current_alignment += calculated_size;

            return calculated_size;
        }

        constexpr bool IsMultiArrayPrimitive(
            ...)
        {
            return false;
        }

        template <typename _T,
                  typename std::enable_if<std::is_enum<_T>::value ||
                                          std::is_arithmetic<_T>::value>::type * = nullptr>
        constexpr bool IsMultiArrayPrimitive(
            _T const *)
        {
            return true;
        }

        template <typename _T, size_t _N>
        constexpr bool IsMultiArrayPrimitive(
            std::array<_T, _N> const *)
        {
            return IsMultiArrayPrimitive(static_cast<_T const *>(nullptr));
        }

        template <class _T, std::size_t _Size>
        std::size_t CalculateSerializedSize(const std::array<_T, _Size> &data, std::size_t &current_alignment)
        {
            std::size_t initial_alignment{current_alignment};

            if (CdrVersion::XCDRv2 == mCdrVersion &&
                !IsMultiArrayPrimitive(&data))
            {
                // DHEADER
                current_alignment += 4 + Alignment(current_alignment, 4);
            }

            std::size_t calculated_size{current_alignment - initial_alignment};
            calculated_size += CalculateArraySerializedSize(data.data(), data.size(), current_alignment);

            if (CdrVersion::XCDRv2 == mCdrVersion &&
                !IsMultiArrayPrimitive(&data))
            {
                // Inform DHEADER can be joined with NEXTINT
                serialized_member_size_ = SERIALIZED_MEMBER_SIZE;
            }

            return calculated_size;
        }

        template <class _T>
        std::size_t CalculateArraySerializedSize(const _T *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            std::size_t calculated_size{0};

            for (std::size_t count = 0; count < num_elements; ++count)
            {
                calculated_size += CalculateSerializedSize(data[count], current_alignment);
            }

            return calculated_size;
        }

        template <class _K, class _V, typename std::enable_if<!std::is_enum<_V>::value && !std::is_arithmetic<_V>::value>::type * = nullptr>
        size_t CalculateSerializedSize(
            const std::map<_K, _V> &data,
            size_t &current_alignment)
        {
            size_t initial_alignment{current_alignment};

            if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                // DHEADER
                current_alignment += 4 + Alignment(current_alignment, 4);
            }

            current_alignment += 4 + Alignment(current_alignment, 4);

            size_t calculated_size{current_alignment - initial_alignment};
            for (auto it = data.begin(); it != data.end(); ++it)
            {
                calculated_size += CalculateSerializedSize(it->first, current_alignment);
                calculated_size += CalculateSerializedSize(it->second, current_alignment);
            }

            if (CdrVersion::XCDRv2 == mCdrVersion)
            {
                // Inform DHEADER can be joined with NEXTINT
                serialized_member_size_ = SERIALIZED_MEMBER_SIZE;
            }

            return calculated_size;
        }

        template <class _K, class _V, typename std::enable_if<std::is_enum<_V>::value || std::is_arithmetic<_V>::value>::type * = nullptr>
        size_t CalculateSerializedSize(
            const std::map<_K, _V> &data,
            size_t &current_alignment)
        {
            size_t initial_alignment{current_alignment};

            current_alignment += 4 + Alignment(current_alignment, 4);

            size_t calculated_size{current_alignment - initial_alignment};
            for (auto it = data.begin(); it != data.end(); ++it)
            {
                calculated_size += CalculateSerializedSize(it->first, current_alignment);
                calculated_size += CalculateSerializedSize(it->second, current_alignment);
            }

            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const int8_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            current_alignment += num_elements;
            return num_elements;
        }

        std::size_t CalculateArraySerializedSize(const uint8_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            current_alignment += num_elements;
            return num_elements;
        }

        std::size_t CalculateArraySerializedSize(const char *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            current_alignment += num_elements;
            return num_elements;
        }

        std::size_t CalculateArraySerializedSize(const wchar_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 2 + Alignment(current_alignment, 2)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const int16_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 2 + Alignment(current_alignment, 2)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const uint16_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 2 + Alignment(current_alignment, 2)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const int32_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 4 + Alignment(current_alignment, 4)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const uint32_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 4 + Alignment(current_alignment, 4)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const int64_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 8 + Alignment(current_alignment, align64_)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const uint64_t *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 8 + Alignment(current_alignment, align64_)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const float *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 4 + Alignment(current_alignment, 4)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        std::size_t CalculateArraySerializedSize(const double *data, std::size_t num_elements, std::size_t &current_alignment)
        {
            static_cast<void>(data);
            std::size_t calculated_size{num_elements * 8 + Alignment(current_alignment, align64_)};
            current_alignment += calculated_size;
            return calculated_size;
        }

        template <class _T>
        std::size_t CalculateMemberSerializedSize(const MemberId &id, const _T &data, std::size_t &current_alignment)
        {
            size_t initial_alignment{current_alignment};

            if (EncodingAlgorithmFlag::PL_CDR == mEncodingAlgorithmFlag ||
                EncodingAlgorithmFlag::PL_CDR2 == mEncodingAlgorithmFlag)
            {
                // Align to 4 for the XCDR header before calculating the data serialized size.
                current_alignment += Alignment(current_alignment, 4);
            }

            size_t prev_size{current_alignment - initial_alignment};
            size_t extra_size{0};

            if (EncodingAlgorithmFlag::PL_CDR == mEncodingAlgorithmFlag)
            {
                current_alignment = 0;
            }

            size_t calculated_size{CalculateSerializedSize(data, current_alignment)};

            if (CdrVersion::XCDRv2 == mCdrVersion && EncodingAlgorithmFlag::PL_CDR2 == mEncodingAlgorithmFlag &&
                0 < calculated_size)
            {

                if (8 < calculated_size ||
                    (1 != calculated_size && 2 != calculated_size && 4 != calculated_size &&
                     8 != calculated_size))
                {
                    extra_size = 8; // Long EMHEADER.
                    if (NO_SERIALIZED_MEMBER_SIZE != serialized_member_size_)
                    {
                        calculated_size -= 4; // Join NEXTINT and DHEADER.
                    }
                }
                else
                {
                    extra_size = 4; // EMHEADER;
                }
            }
            else if (CdrVersion::XCDRv1 == mCdrVersion && EncodingAlgorithmFlag::PL_CDR == mEncodingAlgorithmFlag &&
                     0 < calculated_size)
            {
                extra_size = 4; // ShortMemberHeader

                if (0x3F00 < id.id || calculated_size > std::numeric_limits<uint16_t>::max())
                {
                    extra_size += 8; // LongMemberHeader
                }
            }

            calculated_size += prev_size + extra_size;
            if (EncodingAlgorithmFlag::PL_CDR != mEncodingAlgorithmFlag)
            {
                current_alignment += extra_size;
            }

            serialized_member_size_ = NO_SERIALIZED_MEMBER_SIZE;

            return calculated_size;
        }
        template <class _T>
        size_t CalculateMemberSerializedSize(
            const MemberId &id,
            const optional<_T> &data,
            size_t &current_alignment)
        {
            size_t initial_alignment = current_alignment;

            if (CdrVersion::XCDRv2 != mCdrVersion ||
                EncodingAlgorithmFlag::PL_CDR2 == mEncodingAlgorithmFlag)
            {
                if (data.has_value() || EncodingAlgorithmFlag::PLAIN_CDR == mEncodingAlgorithmFlag)
                {
                    // Align to 4 for the XCDR header before calculating the data serialized size.
                    current_alignment += Alignment(current_alignment, 4);
                }
            }

            size_t prev_size = {current_alignment - initial_alignment};
            size_t extra_size{0};

            if (CdrVersion::XCDRv1 == mCdrVersion &&
                (data.has_value() || EncodingAlgorithmFlag::PLAIN_CDR == mEncodingAlgorithmFlag))
            {
                current_alignment = 0;
            }

            size_t calculated_size{CalculateSerializedSize(data, current_alignment)};

            if (CdrVersion::XCDRv2 == mCdrVersion && EncodingAlgorithmFlag::PL_CDR2 == mEncodingAlgorithmFlag &&
                0 < calculated_size)
            {
                if (8 < calculated_size)
                {
                    extra_size = 8; // Long EMHEADER.
                    if (NO_SERIALIZED_MEMBER_SIZE != serialized_member_size_)
                    {
                        extra_size -= 4; // Join NEXTINT and DHEADER.
                    }
                }
                else
                {
                    extra_size = 4; // EMHEADER;
                }
            }
            else if (CdrVersion::XCDRv1 == mCdrVersion &&
                     (0 < calculated_size || EncodingAlgorithmFlag::PLAIN_CDR == mEncodingAlgorithmFlag))
            {
                extra_size = 4; // ShortMemberHeader

                if (0x3F00 < id.id || calculated_size > std::numeric_limits<uint16_t>::max())
                {
                    extra_size += 8; // LongMemberHeader
                }
            }

            calculated_size += prev_size + extra_size;
            if (CdrVersion::XCDRv1 != mCdrVersion)
            {
                current_alignment += extra_size;
            }

            return calculated_size;
        }

        template <class _T>
        size_t CalculateSerializedSize(
            const optional<_T> &data,
            size_t &current_alignment)
        {
            size_t initial_alignment = current_alignment;

            if (CdrVersion::XCDRv2 == mCdrVersion &&
                EncodingAlgorithmFlag::PL_CDR2 != mEncodingAlgorithmFlag)
            {
                // Take into account the boolean is_present;
                ++current_alignment;
            }

            size_t calculated_size{current_alignment - initial_alignment};

            if (data.has_value())
            {
                calculated_size += CalculateSerializedSize(data.value(), current_alignment);
            }

            return calculated_size;
        }

        std::size_t BeginCalculateTypeSerializedSize(EncodingAlgorithmFlag new_encoding, std::size_t &current_alignment);

        std::size_t EndCalculateTypeSerializedSize(EncodingAlgorithmFlag new_encoding, std::size_t &current_alignment);

        inline size_t Alignment(
            size_t current_alignment,
            size_t data_size) const
        {
            return (data_size - (current_alignment % data_size)) & (data_size - 1);
        }

    private:
        CdrSizeCalculator() = delete;

        CdrVersion mCdrVersion{CdrVersion::XCDRv2};

        EncodingAlgorithmFlag mEncodingAlgorithmFlag{EncodingAlgorithmFlag::PLAIN_CDR2};

        enum SerializedMemberSizeForNextInt
        {
            NO_SERIALIZED_MEMBER_SIZE,
            SERIALIZED_MEMBER_SIZE,
            SERIALIZED_MEMBER_SIZE_4,
            SERIALIZED_MEMBER_SIZE_8
        }
        //! Specifies if a DHEADER was serialized. Used to calculate XCDRv2 member headers.
        serialized_member_size_{NO_SERIALIZED_MEMBER_SIZE};

        //! Align for types equal or greater than 64bits.
        size_t align64_{4};

        template <class _T, typename std::enable_if<std::is_enum<_T>::value ||
                                                    std::is_arithmetic<_T>::value>::type * = nullptr>
        constexpr SerializedMemberSizeForNextInt get_serialized_member_size() const
        {
            return (1 == sizeof(_T) ? SERIALIZED_MEMBER_SIZE : (4 == sizeof(_T) ? SERIALIZED_MEMBER_SIZE_4 : (8 == sizeof(_T) ? SERIALIZED_MEMBER_SIZE_8 : NO_SERIALIZED_MEMBER_SIZE)));
        }
    };
}
#endif