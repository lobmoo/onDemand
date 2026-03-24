#ifndef TXDDS_RTPS_SequenceNumber_H
#define TXDDS_RTPS_SequenceNumber_H

#include "txdds/RTPS/utils/BitmapRange.h"

#include <cstdint>
#include <ostream>
#include <vector>
#include <array>

namespace BaoSky::rtps
{
    struct SequenceNumber
    {
        SequenceNumber() noexcept
        {
            mHighByte = 0;
            mLowByte = 0;
        }
        SequenceNumber(int32_t highByte, uint32_t lowByte) noexcept
            : mHighByte(highByte), mLowByte(lowByte)
        {
        }
        explicit SequenceNumber(uint64_t sequenceNum) noexcept
        {
            mHighByte = static_cast<int32_t>(sequenceNum >> 32u);
            mLowByte = static_cast<uint32_t>(sequenceNum);
        }
        uint64_t ConvertToUint64() const noexcept
        {
            return (static_cast<uint64_t>(mHighByte) << 32u) + mLowByte;
        }
        SequenceNumber &operator++() noexcept
        {
            ++mLowByte;
            if (mLowByte == 0)
            {
                ++mHighByte;
            }
            return *this;
        }
        SequenceNumber operator++(int) noexcept
        {
            SequenceNumber result(*this);
            ++(*this);
            return result;
        }
        static SequenceNumber Unknown() noexcept
        {
            return SequenceNumber{-1, 0};
        }
        uint64_t to64long() const noexcept
        {
            return (static_cast<uint64_t>(mHighByte) << 32u) + mLowByte;
        }

        int32_t mHighByte;
        uint32_t mLowByte;
    };
    const SequenceNumber SEQUENCENUMBER_UNKNOWN{-1, 0};

    inline std::ostream &operator<<(std::ostream &output, const SequenceNumber &seqNum)
    {
        return output << seqNum.to64long();
    }

    inline std::ostream &operator<<(std::ostream &output, const std::vector<SequenceNumber> &seqNumSet)
    {
        for (const SequenceNumber &sn : seqNumSet)
        {
            output << sn << " ";
        }

        return output;
    }

    inline bool operator==(
        const SequenceNumber &sn1,
        const SequenceNumber &sn2) noexcept
    {
        return (sn1.mLowByte == sn2.mLowByte) && (sn1.mHighByte == sn2.mHighByte);
    }

    inline bool operator!=(
        const SequenceNumber &sn1,
        const SequenceNumber &sn2) noexcept
    {
        return (sn1.mLowByte != sn2.mLowByte) || (sn1.mHighByte != sn2.mHighByte);
    }

    inline bool operator>(
        const SequenceNumber &seq1,
        const SequenceNumber &seq2) noexcept
    {
        if (seq1.mHighByte == seq2.mHighByte)
        {
            return seq1.mLowByte > seq2.mLowByte;
        }

        return seq1.mHighByte > seq2.mHighByte;
    }

    inline bool operator<(
        const SequenceNumber &seq1,
        const SequenceNumber &seq2) noexcept
    {
        if (seq1.mHighByte == seq2.mHighByte)
        {
            return seq1.mLowByte < seq2.mLowByte;
        }

        return seq1.mHighByte < seq2.mHighByte;
    }

    inline bool operator>=(
        const SequenceNumber &seq1,
        const SequenceNumber &seq2) noexcept
    {
        if (seq1.mHighByte == seq2.mHighByte)
        {
            return seq1.mLowByte >= seq2.mLowByte;
        }

        return seq1.mHighByte > seq2.mHighByte;
    }

    inline bool operator<=(
        const SequenceNumber &seq1,
        const SequenceNumber &seq2) noexcept
    {
        if (seq1.mHighByte == seq2.mHighByte)
        {
            return seq1.mLowByte <= seq2.mLowByte;
        }

        return seq1.mHighByte < seq2.mHighByte;
    }

    inline SequenceNumber operator-(
        const SequenceNumber &seq,
        const uint32_t inc) noexcept
    {
        SequenceNumber res(seq.mHighByte, seq.mLowByte - inc);

        if (inc > seq.mLowByte)
        {
            --res.mHighByte;
        }

        return res;
    }

    inline SequenceNumber operator+(
        const SequenceNumber &seq,
        const uint32_t inc) noexcept
    {
        SequenceNumber res(seq.mHighByte, seq.mLowByte + inc);

        if (res.mLowByte < seq.mLowByte)
        {
            ++res.mHighByte;
        }

        return res;
    }

    inline SequenceNumber operator-(
        const SequenceNumber &minuend,
        const SequenceNumber &subtrahend) noexcept
    {
        SequenceNumber res(minuend.mHighByte - subtrahend.mHighByte, minuend.mLowByte - subtrahend.mLowByte);

        if (minuend.mLowByte < subtrahend.mLowByte)
        {
            --res.mHighByte;
        }

        return res;
    }

    struct SequenceNumberDiff
    {
        uint32_t operator()(
            const SequenceNumber &a,
            const SequenceNumber &b) const noexcept
        {
            SequenceNumber diff = a - b;
            return diff.mLowByte;
        }
    };
    typedef BitmapRange<SequenceNumber, SequenceNumberDiff, 256> SequenceNumberSet;

    inline std::ostream &operator<<(
        std::ostream &output,
        const SequenceNumberSet &sns)
    {
        output << sns.base().to64long() << ":";
        sns.for_each([&output](
                         SequenceNumber it)
                     { output << it.to64long() << "-"; });

        return output;
    }

}

#endif