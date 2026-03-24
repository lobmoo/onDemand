#ifndef TXDDS_DCPS_IDATAWRITER_H
#define TXDDS_DCPS_IDATAWRITER_H

#include "txdds/DCPS/core/IDomainEntity.h"
#include "txdds/DCPS/publisher/qos/DataWriterQos.h"

namespace BaoSky::dds
{
    class IDataWriterListener;
    class ITopic;

    class TXDDS_API IDataWriter : public IDomainEntity
    {
    public:
        IDataWriter() : IDomainEntity() {}

        virtual ~IDataWriter() override {}

        virtual ReturnCode Enable() = 0;

        virtual ReturnCode Disable() = 0;

        virtual ReturnCode SetListener(IDataWriterListener *listener, const StatusMask &mask = ALL_STATUS) = 0;

        virtual IDataWriterListener *GetListener() = 0;

        virtual ReturnCode SetQos(const DataWriterQos &qos) = 0;

        virtual ReturnCode GetQos(DataWriterQos &qos) = 0;

        virtual ReturnCode Write(void *data) = 0;

        virtual ReturnCode Write(void *data, const uint64_t &len) = 0;

        virtual ITopic *GetTopic() = 0;
    };
}

#endif