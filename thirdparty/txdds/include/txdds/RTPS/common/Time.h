/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-08-07 10:53:05
 * @FilePath: /TXDDS/include/RTPS/common/Time.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_TIME_T_H
#define TXDDS_RTPS_TIME_T_H
#include <cstdint>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "txdds/txddsexport.h"
namespace BaoSky::rtps
{
    struct TXDDS_API Time
    {
        static constexpr int32_t INFINITE_SECONDS = 0x7fffffff;
        static constexpr uint32_t INFINITE_NANOSECONDS = 0xffffffffu;

        int32_t seconds;
        uint32_t nanosec;

        Time();
        Time(int32_t sec, uint32_t nsec);

        Time(long double sec);

        void Fraction(uint32_t frac);
        uint32_t Fraction() const;
        int64_t ToNs() const;

        inline bool IsInfinite() const noexcept
        {
            return IsInfinite(*this);
        }

        static void Now(Time &ret);

        static inline constexpr bool IsInfinite(const Time &t) noexcept
        {
            return (INFINITE_SECONDS == t.seconds) || (INFINITE_NANOSECONDS == t.nanosec);
        }
    };

    static inline bool operator==(const Time &t1, const Time &t2)
    {
        return (t1.seconds == t2.seconds) && t1.Fraction() == t2.Fraction();
    }

    static inline bool operator!=(const Time &t1, const Time &t2)
    {
        return !(t1 == t2);
    }

    static inline bool operator<(const Time &t1, const Time &t2)
    {
        if (t1.seconds < t2.seconds)
        {
            return true;
        }
        else if (t1.seconds > t2.seconds)
        {
            return false;
        }
        else
        {
            if (t1.Fraction() < t2.Fraction())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    static inline bool operator>(const Time &t1, const Time &t2)
    {
        if (t1.seconds > t2.seconds)
        {
            return true;
        }
        else if (t1.seconds < t2.seconds)
        {
            return false;
        }
        else
        {
            if (t1.Fraction() > t2.Fraction())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    static inline bool operator<=(const Time &t1, const Time &t2)
    {
        return !(t1 > t2);
    }

    static inline bool operator>=(const Time &t1, const Time &t2)
    {
        return !(t1 < t2);
    }

    inline std::ostream &operator<<(std::ostream &output, const Time &t)
    {
        return output << t.seconds << "." << t.nanosec;
    }

    inline std::istream &operator>>(std::istream &input, Time &t)
    {
        std::istream::sentry s(input);
        if (s)
        {
            char point;
            int32_t sec = 0;
            uint32_t nano = 0;
            std::ios_base::iostate excp_mask = input.exceptions();
            try
            {
                input.exceptions(excp_mask | std::ios_base::failbit | std::ios_base::badbit);

                input >> sec;
                input >> point >> nano;
                if (point != '.' || nano >= 1000000000)
                {
                    input.setstate(std::ios_base::failbit);
                    nano = 0;
                }
            }
            catch (std::ios_base::failure &)
            {
            }
            t.seconds = sec;
            t.nanosec = nano;

            input.exceptions(excp_mask);
        }

        return input;
    }

    static inline Time operator+(const Time &ta, const Time &tb)
    {
        Time result(ta.seconds + tb.seconds, ta.Fraction() + tb.Fraction());
        if (result.Fraction() < ta.Fraction())
        {
            ++result.seconds;
        }
        return result;
    }

    static inline Time operator-(const Time &ta, const Time &tb)
    {
        Time result(ta.seconds - tb.seconds, ta.Fraction() - tb.Fraction());
        if (result.Fraction() > ta.Fraction())
        {
            --result.seconds;
        }
        return result;
    }

    const Time TIME_INVALID{-1, 0xffffffff};
}

#endif