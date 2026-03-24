#ifndef _SUBSCRIPTION_MATCHED_STATUS_HPP_
#define _SUBSCRIPTION_MATCHED_STATUS_HPP_

#include <cstdint>

#include "txdds/DCPS/core/status/MatchedStatus.h"
#include "txdds/DCPS/common/InstanceHandle.h"

namespace BaoSky::dds
{
    struct SubscriptionMatchedStatus : public MatchedStatus
    {
        InstanceHandle last_publication_handle;
    };

}

#endif
