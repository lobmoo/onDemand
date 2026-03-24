#ifndef TXDDS_RTPS_READERATTRIBUTES_H
#define TXDDS_RTPS_READERATTRIBUTES_H

#include "txdds/RTPS/attributes/EndpointAttributes.h"
#include "txdds/RTPS/common/Duration.h"

namespace BaoSky::rtps
{
    class ReaderAttributes
    {
    public:
        ReaderAttributes() = default;

        virtual ~ReaderAttributes() = default;

        EndpointAttributes endpoint;

        Duration heartbeatResponseDelay;

        Duration hearbeatSuppressionDuration;

        bool expectsInlineQos = false;
    };
}

#endif