#ifndef TXDDS_DCPS_BASESTATUS_H
#define TXDDS_DCPS_BASESTATUS_H

#include <cstdint>

namespace BaoSky::dds
{

    struct BaseStatus
    {

        int32_t total_count = 0;

        int32_t total_count_change = 0;
    };

    using SampleLostStatus = BaseStatus;
    using LivelinessLostStatus = BaseStatus;
    using InconsistentTopicStatus = BaseStatus;

}
#endif
