#ifndef TXDDS_DCPS_MATCHEDSTATUS_H
#define TXDDS_DCPS_MATCHEDSTATUS_H

#include <cstdint>

namespace BaoSky::dds
{
    struct MatchedStatus
    {
        MatchedStatus() = default;

        ~MatchedStatus() = default;

        int32_t total_count = 0;
        int32_t total_count_change = 0;
        int32_t current_count = 0;
        int32_t current_count_change = 0;
    };
}

#endif
