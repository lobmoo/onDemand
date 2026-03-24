#ifndef TXDDS_DCPS_INCOMPATIBLEQOSSTATUS_H
#define TXDDS_DCPS_INCOMPATIBLEQOSSTATUS_H

#include <txdds/DCPS/core/policy/QosPolicy.h>

#include <cstdint>
#include <vector>

namespace BaoSky::dds
{
    struct QosPolicyCount
    {
        QosPolicyCount() = default;
        QosPolicyCount(QosPolicyId_t id, int32_t c) : policy_id(id), count(c)
        {
        }

        QosPolicyId_t policy_id = INVALID_QOS_POLICY_ID;
        uint32_t count = 0;
    };

    struct IncompatibleQosStatus
    {
        uint32_t total_count = 0;
        uint32_t total_count_change = 0;
        QosPolicyId_t last_policy_id = INVALID_QOS_POLICY_ID;
        QosPolicyCountSeq policies;

        IncompatibleQosStatus() : policies(static_cast<size_t>(NEXT_QOS_POLICY_ID), QosPolicyCount(INVALID_QOS_POLICY_ID, 0))
        {
            for (uint32_t id = 0; id < NEXT_QOS_POLICY_ID; ++id)
            {
                policies[id].policy_id = static_cast<QosPolicyId_t>(id);
                policies[id].count = 0;
            }
        }
    };

    using QosPolicyCountSeq = std::vector<QosPolicyCount>;
    using RequestedIncompatibleQosStatus = IncompatibleQosStatus;
    using OfferedIncompatibleQosStatus = IncompatibleQosStatus;
}

#endif