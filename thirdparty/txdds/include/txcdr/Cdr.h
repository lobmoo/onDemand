/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-02-17 13:01:27
 * @FilePath     : /txcdr/include/Cdr.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXCDR_CDR_H
#define TXCDR_CDR_H

#include <vector>
#include <string>
#include "txdds/txddsexport.h"
#include "txcdr/TXBuffer.h"
#include "txcdr/ReturnCode.h"
#include "txcdr/CdrSizeCalculator.h"
#include "txcdr/MemberId.h"
namespace BaoSky::Cdr
{

    class TXDDS_API Cdr
    {
    public:
        // Default endianess in the system.
#if TXCDR_IS_BIG_ENDIAN_TARGET
        inline static const Endianness DEFAULT_ENDIAN = BIG_ENDIANNESS;
#else
        inline static const Endianness DEFAULT_ENDIAN = LITTLE_ENDIANNESS;
#endif // if TXCDR_IS_BIG_ENDIAN_TARGET
        typedef enum
        {
            //! Initially a short member header is allocated and cannot be changed. This option may cause an exception.
            SHORT_HEADER,
            //! Initially a long member header is allocated and cannot be changed.
            LONG_HEADER,
            //! Initially a short member header is allocated but can be changed to the longer version.
            AUTO_WITH_SHORT_HEADER_BY_DEFAULT,
            //! Initially a long member header is allocated but can be changed to the shorter version.
            AUTO_WITH_LONG_HEADER_BY_DEFAULT
        } XCdrHeaderSelection;

        class state
        {
            friend class Cdr;

        public:
            state(
                const Cdr &cdr)
                : offset_(cdr.mOffsetIter),
                  origin_(cdr.mOriginIter),
                  swap_bytes_(cdr.mSwapBytes),
                  last_data_size_(cdr.mLastDataSize),
                  next_member_id_(cdr.mNextMemberId),
                  previous_encoding_(cdr.mEncodingAlgorithmFlag)
            {
            }

            //! Copy constructor.
            state(
                const state &state);

            //! Compares two states.
            bool operator==(
                const state &other_state) const;

            state &operator=(
                const state &state) = delete;

            //! The position in the buffer when the state was created.
            const TXBufferIterator offset_;

            //! The position from the alignment is calculated, when the state was created.
            const TXBufferIterator origin_;

            //! This attribute specifies if it is needed to swap the bytes when the state is created.
            bool swap_bytes_{false};

            //! Stores the last datasize serialized/deserialized when the state was created.
            size_t last_data_size_{0};

            //! Not related with the state. Next member id which will be encoded.
            MemberId next_member_id_;

            //! Not related with the state. Used by encoding algorithms to set the encoded member size.
            uint32_t member_size_{0};

            //! Not related with the state. Used by encoding algorithms to store the selected member header version.
            XCdrHeaderSelection header_selection_{XCdrHeaderSelection::AUTO_WITH_SHORT_HEADER_BY_DEFAULT};

            //! Not related with the state. Used by encoding algorithms to store the allocated member header version.
            XCdrHeaderSelection header_serialized_{XCdrHeaderSelection::SHORT_HEADER};

            //! Not related with the state. Used by encoding algorithms to store the previous encoding algorithm.
            EncodingAlgorithmFlag previous_encoding_{EncodingAlgorithmFlag::PLAIN_CDR2};
        };

        void SetState(
            const state &current_state)
        {
            mOffsetIter >> current_state.offset_;
            mOriginIter >> current_state.origin_;
            mSwapBytes = current_state.swap_bytes_;
            mLastDataSize = current_state.last_data_size_;
            mNextMemberId = current_state.next_member_id_;
        }

        Cdr(TXBuffer &cdr_buffer, const Endianness endianness = DEFAULT_ENDIAN, const CdrVersion cdr_version = XCDRv2);

        size_t GetSerializedDataLength() { return mOffsetIter - mCdrBuffer.begin(); };

        bool Resize(size_t min_size_inc);

        inline size_t Alignment(size_t data_size)
        {
            return data_size > mLastDataSize ? (data_size - ((mOffsetIter - mOriginIter) % data_size)) & (data_size - 1) : 0;
        }
        inline static size_t Alignment(
            size_t current_alignment,
            size_t data_size)
        {
            return (data_size - (current_alignment % data_size)) & (data_size - 1);
        }

        inline void MakeAlignment(size_t align)
        {
            mOffsetIter += align;
            mLastDataSize = 0;
        }
        void SetEncodingFlag(EncodingAlgorithmFlag encodingFlag)
        {
            mEncodingAlgorithmFlag = encodingFlag;
        }

        bool Jump(size_t numBytes);

        inline void ResetAlignment()
        {
            mOriginIter = mOffsetIter;
            mLastDataSize = 0;
        }

        inline Endianness endianness()
        {
            return (Endianness)mEndianness;
        }

        CdrVersion GetCdrVersion();

    protected:
        uint8_t mEndianness{Endianness::LITTLE_ENDIANNESS};

        CdrVersion mCdrVersion{CdrVersion::XCDRv2};

        EncodingAlgorithmFlag mEncodingAlgorithmFlag{EncodingAlgorithmFlag::PLAIN_CDR2};

        // The current position in the serialization/deserialization process.
        TXBufferIterator mOffsetIter;
        TXBufferIterator mPreviousOffsetIter;

        // The position from where the alignment is calculated.
        TXBufferIterator mOriginIter;

        // The last position in the buffer;
        TXBufferIterator mEndIter;

        // Reference to the buffer that will be serialized/deserialized.
        TXBuffer &mCdrBuffer;

        // This attribute specifies if it is needed to swap the bytes when the state is created.
        bool mSwapBytes{false};

        // Stores the last datasize serialized/deserialized when the state was created.
        size_t mLastDataSize{0};

        // Align for types equal or greater than 64bits.
        size_t mAlign64{4};

        // This attribute stores the option flags when the CDR type is DDS_CDR;
        std::array<uint8_t, 2> mOptions{{0}};
        MemberId mNextMemberId;
    };
}
#endif
