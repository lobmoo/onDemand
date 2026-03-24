/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-02-17 13:07:27
 * @FilePath     : /txcdr/include/TXBuffer.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXCDR_TXBUFFER_H
#define TXCDR_TXBUFFER_H
#include "txdds/txddsexport.h"
#include <memory>
#include <stdio.h>
template <class D, class S, class N>
void memcpy(D des, S src, N n)
{
    char *xxd = (char *)(des);
    const char *xxs = (const char *)(src);
    int xxn = (n);
    while (xxn-- > 0)
    {
        *(xxd++) = *(xxs++);
    }
}

inline uint32_t size_to_uint32(
    size_t val)
{
#if defined(_WIN32) || !defined(FASTCDR_ARM32)
    // On 64 bit platforms and all Windows architectures (because of C4267), explicitly cast.
    return static_cast<uint32_t>(val);
#else
    // Skip useless cast on 32-bit builds.
    return val;
#endif // if defined(_WIN32) || !defined(FASTCDR_ARM32)
}
namespace BaoSky::Cdr
{

    class TXDDS_API TXBufferIterator
    {
    public:
        TXBufferIterator() = default;

        TXBufferIterator(char *buffer, size_t index)
            : mBuffer(buffer), mCurrentPosition(&mBuffer[index])
        {
        }

        template <typename _T>
        inline void operator<<(const _T &data)
        {
            memcpy(mCurrentPosition, &data, sizeof(_T));
        }

        template <typename _T>
        inline void operator>>(const _T &data)
        {
            memcpy(&data, mCurrentPosition, sizeof(_T));
        }

        inline size_t operator-(const TXBufferIterator &it) const
        {
            return static_cast<size_t>(mCurrentPosition - it.mCurrentPosition);
        }

        inline TXBufferIterator operator++()
        {
            ++mCurrentPosition;
            return *this;
        }

        inline TXBufferIterator operator++(int)
        {
            TXBufferIterator tmp = *this;
            ++*this;
            return tmp;
        }

        inline void operator+=(size_t numBytes)
        {
            mCurrentPosition += numBytes;
        }

        inline void memcopy(const void *src, const size_t size)
        {
            if (size > 0)
            {
                memcpy(mCurrentPosition, src, size);
            }
        }
        inline void operator<<(
            const TXBufferIterator &iterator)
        {
            ptrdiff_t diff = mCurrentPosition - mBuffer;
            mBuffer = iterator.mBuffer;
            mCurrentPosition = mBuffer + diff;
        }

        /*!
         * @brief This operator changes the position where the iterator points.
         * This operator takes the index of the source iterator, but the iterator continues using its raw buffer.
         * @param iterator The source iterator. The iterator will use the source's iterator index to point to its own raw buffer.
         */
        inline void operator>>(
            const TXBufferIterator &iterator)
        {
            ptrdiff_t diff = iterator.mCurrentPosition - iterator.mBuffer;
            mCurrentPosition = mBuffer + diff;
        }

        inline char *operator&()
        {
            return mCurrentPosition;
        }

        bool operator==(const TXBufferIterator &other_iterator) const
        {
            return other_iterator.mCurrentPosition == mCurrentPosition;
        }

        bool operator!=(const TXBufferIterator &other_iterator) const
        {
            return !(other_iterator == *this);
        }

    private:
        // Pointer to the raw buffer.
        char *mBuffer{nullptr};

        // Current position in the raw buffer.
        char *mCurrentPosition{nullptr};
    };

    class TXDDS_API TXBuffer
    {
    public:
        inline TXBufferIterator begin()
        {
            return (TXBufferIterator(mBuffer, 0));
        }
        inline TXBufferIterator end()
        {
            return (TXBufferIterator(mBuffer, mSize));
        }
        inline void DumpBuffer()
        {
            FILE *f = fopen("/data/BaoskyRuntime/log/error.bin", "wb");
            if (!f)
            {
                return;
            }
            fwrite(mBuffer, 1, mSize, f);
            fflush(f);
            fclose(f);
        }
        bool Resize(size_t incrementSize);

        TXBuffer() = default;
        TXBuffer(char *const buffer, const size_t bufferSize)
            : mBuffer(buffer), mSize(bufferSize), mInternalBuffer(false)
        {
        }
        virtual ~TXBuffer();

    private:
        // Pointer to the stream of bytes that contains the serialized data.
        char *mBuffer{nullptr};

        // The total size of the user's buffer.
        size_t mSize{0};

        // This variable indicates if the managed buffer is internal or is from the user.
        bool mInternalBuffer{true};
    };
}
#endif