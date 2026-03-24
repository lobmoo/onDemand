#ifndef TXDDS_RTPS_MATCHINGINFO_H
#define TXDDS_RTPS_MATCHINGINFO_H

#include "txdds/RTPS/common/Guid.h"

namespace BaoSky::rtps
{
    enum MatchingStatus
    {
        MATCHED_MATCHING,
        REMOVED_MATCHING
    };

    struct MatchingInfo
    {
    public:
        MatchingInfo() = default;
        virtual ~MatchingInfo() = default;

        MatchingStatus status;
        BaoSky::rtps::Guid remoteEndpointGuid;
    };
}

#endif