#ifndef TXDDS_DCPS_ISUBSCRIBERLISTENER_H
#define TXDDS_DCPS_ISUBSCRIBERLISTENER_H

#include "txdds/DCPS/subscriber/IDataReaderListener.h"

namespace BaoSky::dds
{
    class ISubscriberListener : public IDataReaderListener
    {
    public:
        virtual ~ISubscriberListener() override {}
    };
}

#endif