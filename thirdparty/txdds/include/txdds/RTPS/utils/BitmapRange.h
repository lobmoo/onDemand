#ifndef TXDDS_RTPS_BITMAPRANGE_H
#define TXDDS_RTPS_BITMAPRANGE_H

#include <array>
#include <cstdint>
#include <string.h>
#include <limits>
#ifdef _WIN32
#include <intrin.h>
#endif

namespace BaoSky::rtps
{

    using std::uint32_t;

    template <class T>
    struct DiffFunction
    {
        constexpr auto operator()(T a, T b) const -> decltype(a - b)
        {
            return a - b;
        }
    };

    template <class T, class Diff = DiffFunction<T>, uint32_t NBITS = 256>
    class BitmapRange
    {
#define NITEMS ((NBITS + 31u) / 32u)
    public:
        using bitmap_type = std::array<uint32_t, NITEMS>;
        BitmapRange() noexcept
            : base_(), range_max_(base_ + (NBITS - 1)), bitmap_(), num_bits_(0u)
        {
        }

        explicit BitmapRange(T base) noexcept : BitmapRange(base, NBITS - 1)
        {
        }

        BitmapRange(T base, uint32_t max_bits) noexcept
            : base_(base), range_max_(base_ + (std::min)(max_bits, NBITS - 1)), bitmap_(), num_bits_(0u)
        {
        }

        T base() const noexcept
        {
            return base_;
        }
        void base(T base) noexcept
        {
            base_ = base;
            range_max_ = base_ + (NBITS - 1);
            num_bits_ = 0;
            bitmap_.fill(0u);
        }
        void base(T base, uint32_t max_bits) noexcept
        {
            base_ = base;
            range_max_ = base_ + (std::min)(max_bits, NBITS - 1);
            num_bits_ = 0;
            bitmap_.fill(0u);
        }

        void base_update(T base) noexcept
        {
            if (base == base_)
            {
                return;
            }

            Diff d_func;
            if (base > base_)
            {
                uint32_t n_bits = d_func(base, base_);
                shift_map_left(n_bits);
            }
            else
            {
                uint32_t n_bits = d_func(base_, base);
                shift_map_right(n_bits);
            }
            base_ = base;
            range_max_ = base_ + (NBITS - 1);
        }
        bool empty() const noexcept
        {
            return num_bits_ == 0u;
        }

        T max() const noexcept
        {
            return base_ + (num_bits_ - 1);
        }

        T min() const noexcept
        {
            T item = base_;
            uint32_t n_longs = (num_bits_ + 31u) / 32u;
            for (uint32_t i = 0; i < n_longs; i++)
            {
                uint32_t bits = bitmap_[i];
                if (bits)
                {
                    uint32_t offset = static_cast<uint32_t>(__builtin_clz(bits));
                    return item + offset;
                }
                item = item + 32u;
            }

            return base_;
        }

        bool is_set(const T& item) const noexcept
        {
            if ((item >= base_) && (range_max_ >= item))
            {
                Diff d_func;
                uint32_t diff = d_func(item, base_);
                if (diff < num_bits_)
                {
                    uint32_t pos = diff >> 5;
                    diff &= 31u;
                    return (bitmap_[pos] & (1u << (31u - diff))) != 0;
                }
            }

            return false;
        }

        bool add(const T& item) noexcept
        {
            if ((item >= base_) && (range_max_ >= item))
            {
                Diff d_func;
                uint32_t diff = d_func(item, base_);
                num_bits_ = std::max(diff + 1, num_bits_);
                uint32_t pos = diff >> 5;
                diff &= 31u;
                bitmap_[pos] |= (1u << (31u - diff));
                return true;
            }

            return false;
        }

        void add_range(const T& from, const T& to)
        {
            constexpr uint32_t full_mask = (std::numeric_limits<uint32_t>::max)();
            T min = (base_ >= from) ? base_ : from;
            T max = (to >= base_ + NBITS) ? base_ + NBITS : to;
            if (min >= max)
            {
                return;
            }
            Diff d_func;
            uint32_t offset = d_func(min, base_);
            uint32_t n_bits = d_func(max, min);
            num_bits_ = std::max(num_bits_, offset + n_bits);
            uint32_t pos = offset >> 5;
            offset &= 31u;
            uint32_t mask = full_mask;
            mask >>= offset;
            uint32_t bits_in_mask = 32u - offset;
            while (n_bits >= bits_in_mask)
            {
                bitmap_[pos] |= mask;
                pos++;
                n_bits -= bits_in_mask;
                mask = full_mask;
                bits_in_mask = 32u;
            }
            if (n_bits > 0)
            {
                bitmap_[pos] |= mask & (full_mask << (bits_in_mask - n_bits));
            }
        }

        void remove(const T& item) noexcept
        {
            T max_value = max();
            if ((item >= base_) && (max_value >= item))
            {
                Diff d_func;
                uint32_t diff = d_func(item, base_);
                uint32_t pos = diff >> 5;
                diff &= 31u;
                bitmap_[pos] &= ~(1u << (31u - diff));

                if (item == max_value)
                {
                    calc_maximum_bit_set(pos + 1, 0);
                }
            }
        }

        void bitmap_get(uint32_t& num_bits, bitmap_type& bitmap, uint32_t& num_longs_used) const noexcept
        {
            num_bits = num_bits_;
            num_longs_used = (num_bits_ + 31u) / 32u;
            bitmap = bitmap_;
        }

        uint32_t num_longs_used()
        {
            return (num_bits_ + 31u) / 32u;
        }

        T range_max()
        {
            return range_max_;
        }

        uint32_t count()
        {
            uint32_t result = 0;
            T item = base_;
            uint32_t n_longs = (num_bits_ + 31u) / 32u;
            for (uint32_t i = 0; i < n_longs; i++)
            {
                uint32_t bits = bitmap_[i];
                while (bits)
                {
#ifdef _WIN32
                    unsigned long bit;
                    _BitScanReverse(&bit, bits);
                    uint32_t offset = 31u ^ bit;
#else
                    uint32_t offset = static_cast<uint32_t>(__builtin_clz(bits));
                    uint32_t bit = 31u ^ offset;
#endif // ifdef _WIN32

                    result++;
                    bits &= ~(1u << bit);
                }
                item = item + 32u;
            }
            return result;
        }

        void bitmap_set(uint32_t num_bits, const uint32_t* bitmap) noexcept
        {
            num_bits_ = std::min(num_bits, NBITS);
            uint32_t num_items = ((num_bits_ + 31u) / 32u);
            uint32_t num_bytes = num_items * static_cast<uint32_t>(sizeof(uint32_t));
            bitmap_.fill(0u);
            memcpy(bitmap_.data(), bitmap, num_bytes);
            short shift = num_bits & 31u;
            if (0 < num_bits && shift != 0)
            {
                bitmap_[num_items - 1] &= ~((std::numeric_limits<uint32_t>::max)() >> shift);
            }
            calc_maximum_bit_set(num_items, 0);
        }
        template <class UnaryFunc>
        void for_each(UnaryFunc f) const
        {
            T item = base_;

            uint32_t n_longs = (num_bits_ + 31u) / 32u;
            for (uint32_t i = 0; i < n_longs; i++)
            {
                uint32_t bits = bitmap_[i];
                while (bits)
                {
#ifdef _WIN32
                    unsigned long bit;
                    _BitScanReverse(&bit, bits);
                    uint32_t offset = 31u ^ bit;
#else
                    uint32_t offset = static_cast<uint32_t>(__builtin_clz(bits));
                    uint32_t bit = 31u ^ offset;
#endif // _WIN32
                    f(item + offset);
                    bits &= ~(1u << bit);
                }
                item = item + 32u;
            }
        }

    protected:
        T base_;
        T range_max_;
        bitmap_type bitmap_;
        uint32_t num_bits_;

    private:
        void shift_map_left(uint32_t n_bits)
        {
            if (n_bits >= num_bits_)
            {
                num_bits_ = 0;
                bitmap_.fill(0u);
            }
            else
            {
                num_bits_ -= n_bits;
                uint32_t n_items = n_bits >> 5;
                n_bits &= 31u;
                if (n_bits == 0)
                {
                    std::copy(bitmap_.begin() + n_items, bitmap_.end(), bitmap_.begin());
                    std::fill_n(bitmap_.rbegin(), n_items, 0);
                }
                else
                {
                    uint32_t overflow_bits = 32u - n_bits;
                    size_t last_index = NITEMS - 1u;
                    for (size_t i = 0, n = n_items; n < last_index; i++, n++)
                    {
                        bitmap_[i] = (bitmap_[n] << n_bits) | (bitmap_[n + 1] >> overflow_bits);
                    }
                    bitmap_[last_index - n_items] = bitmap_[last_index] << n_bits;
                    std::fill_n(bitmap_.rbegin(), n_items, 0);
                }
            }
        }

        void shift_map_right(uint32_t n_bits)
        {
            if (n_bits >= NBITS)
            {
                num_bits_ = 0;
                bitmap_.fill(0u);
            }
            else
            {
                uint32_t new_num_bits = num_bits_ + n_bits;
                bool find_new_max = new_num_bits > NBITS;
                uint32_t n_items = n_bits >> 5;
                n_bits &= 31u;
                if (n_bits == 0)
                {
                    std::copy(bitmap_.rbegin() + n_items, bitmap_.rend(), bitmap_.rbegin());
                    std::fill_n(bitmap_.begin(), n_items, 0);
                }
                else
                {
                    uint32_t overflow_bits = 32u - n_bits;
                    size_t last_index = NITEMS - 1u;
                    for (size_t i = last_index, n = last_index - n_items; n > 0; i--, n--)
                    {
                        bitmap_[i] = (bitmap_[n] >> n_bits) | (bitmap_[n - 1] << overflow_bits);
                    }
                    bitmap_[n_items] = bitmap_[0] >> n_bits;
                    std::fill_n(bitmap_.begin(), n_items, 0);
                }

                num_bits_ = new_num_bits;
                if (find_new_max)
                {
                    calc_maximum_bit_set(NITEMS, n_items);
                }
            }
        }

        void calc_maximum_bit_set(uint32_t starting_index, uint32_t min_index)
        {
            num_bits_ = 0;
            for (uint32_t i = starting_index; i > min_index;)
            {
                --i;
                uint32_t bits = bitmap_[i];
                if (bits != 0)
                {
                    bits = (bits & ~(bits - 1));
#ifdef _WIN32
                    unsigned long bit;
                    _BitScanReverse(&bit, bits);
                    uint32_t offset = (31u ^ bit) + 1;
#else
                    uint32_t offset = static_cast<uint32_t>(__builtin_clz(bits)) + 1u;
#endif // ifdef _WIN32
                    num_bits_ = (i << 5u) + offset;
                    break;
                }
            }
        }
    };

}

#endif
