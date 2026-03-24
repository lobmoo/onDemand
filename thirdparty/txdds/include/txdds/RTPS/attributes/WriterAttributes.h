#ifndef TXDDS_RTPS_WRITERATTRIBUTES_H
#define TXDDS_RTPS_WRITERATTRIBUTES_H

#include "txdds/RTPS/attributes/EndpointAttributes.h"
#include "txdds/RTPS/common/Duration.h"
#include "txdds/RTPS/common/SequenceNumber.h"
#include <txdds/DCPS/core/policy/QosPolicy.h>

namespace BaoSky::rtps
{
    class WriterAttributes
    {
    public:
        WriterAttributes() = default;

        virtual ~WriterAttributes() = default;

        EndpointAttributes endpoint;

        Duration heartbeatPeriod;

        Duration nackResponseDelay;

        Duration nackSuppressionDuration;

        SequenceNumber lastChangeSequenceNumber;

        long dataMaxSizeSerialized;

        BaoSky::dds::HistoryQosPolicy history_;

        BaoSky::dds::ResourceLimitsQosPolicy resource_limits_;
    };
}

#endif