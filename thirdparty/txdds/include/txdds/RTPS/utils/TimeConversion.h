#ifndef TXDDS_RTPS_TIMECONVERSION_H
#define TXDDS_RTPS_TIMECONVERSION_H

#include "txdds/RTPS/common/Time.h"
#include "txdds/RTPS/common/Duration.h"

#include <cstdint>
#include <cmath>

namespace BaoSky::rtps
{
    namespace TimeConv
    {

        inline double Time_t2SecondsDouble(const Time &t)
        {
            return (double)t.seconds + (double)(t.Fraction() / pow(2.0, 32));
        }

        inline int64_t Time_t2MicroSecondsInt64(const Time &t)
        {
            return (int64_t)(t.Fraction() / pow(2.0, 32) * pow(10.0, 6)) + t.seconds * (int64_t)pow(10.0, 6);
        }

        inline int64_t Duration_t2MicroSecondsInt64(const Duration &t)
        {
            return (int64_t)(t.nanosec / 1000.0) + t.seconds * (int64_t)pow(10.0, 6);
        }

        inline double Time_t2MicroSecondsDouble(const Time &t)
        {
            return ((double)t.Fraction() / pow(2.0, 32) * pow(10.0, 6)) + (double)t.seconds * pow(10.0, 6);
        }

        inline int64_t Time_t2MilliSecondsInt64(const Time &t)
        {
            return (int64_t)(t.Fraction() / pow(2.0, 32) * pow(10.0, 3)) + t.seconds * (int64_t)pow(10.0, 3);
        }

        inline double Time_t2MilliSecondsDouble(const Time &t)
        {
            return ((double)t.Fraction() / pow(2.0, 32) * pow(10.0, 3)) + (double)t.seconds * pow(10.0, 3);
        }

        inline double Duration_t2MilliSecondsDouble(const Duration &t)
        {
            return ((double)t.nanosec / 1000000.0) + (double)t.seconds * pow(10.0, 3);
        }

        inline Time MilliSeconds2Time_t(double millisec)
        {
            Time t;
            t.seconds = (int32_t)(millisec / pow(10.0, 3));
            t.Fraction((uint32_t)((millisec - (double)t.seconds * pow(10.0, 3)) / pow(10.0, 3) * pow(2.0, 32)));
            return t;
        }

        inline Time MicroSeconds2Time_t(double microsec)
        {
            Time t;
            t.seconds = (int32_t)(microsec / pow(10.0, 6));
            t.Fraction((uint32_t)((microsec - (double)t.seconds * pow(10.0, 6)) / pow(10.0, 6) * pow(2.0, 32)));
            return t;
        }

        inline Time Seconds2Time_t(double seconds)
        {
            Time t;
            t.seconds = (int32_t)seconds;
            t.Fraction((uint32_t)((seconds - (double)t.seconds) * pow(2.0, 32)));
            return t;
        }

        inline double Time_tAbsDiff2DoubleMillisec(const Time &t1, const Time &t2)
        {
            double result = 0;
            result += (double)abs((t2.seconds - t1.seconds) * 1000);
            result += (double)std::abs((t2.Fraction() - t1.Fraction()) / pow(2.0, 32) * 1000);
            return result;
        }

        inline Time MilliSecondsWithRandOffset2Time_t(double millisec, double randoff)
        {
            randoff = std::abs(randoff);
            millisec = millisec + (-randoff) + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (2 * randoff)));
            return MilliSeconds2Time_t(millisec);
        }

        inline Time MicroSecondsWithRandOffset2Time_t(double microsec, double randoff)
        {
            randoff = std::abs(randoff);
            microsec = microsec + (-randoff) + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (2 * randoff)));
            return MicroSeconds2Time_t(microsec);
        }

        inline Time SecondsWithRandOffset2Time_t(double sec, double randoff)
        {
            randoff = std::abs(randoff);
            sec = sec + (-randoff) + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (2 * randoff)));
            return Seconds2Time_t(sec);
        }
    }
}

#endif
