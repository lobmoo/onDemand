#ifndef TXDDS_DCPS_IPUBLISHERLISTENER_H
#define TXDDS_DCPS_IPUBLISHERLISTENER_H

#include "txdds/DCPS/publisher/IDataWriterListener.h"

namespace BaoSky::dds
{
    class TXDDS_API IPublisherListener : public IDataWriterListener
    {
    public:
        virtual ~IPublisherListener() override {}
    };
}

#endif