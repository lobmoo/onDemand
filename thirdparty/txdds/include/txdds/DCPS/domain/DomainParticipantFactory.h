#ifndef TXDDS_DCPS_DOMAINPARTICIPANTFACTORY_H
#define TXDDS_DCPS_DOMAINPARTICIPANTFACTORY_H

#include "txdds/DCPS/common/DomainId.h"
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/common/StatusKind.h"
#include "txdds/DCPS/domain/qos/DomainParticipantQos.h"
#include "txdds/txddsexport.h"

#include <vector>
#include <mutex>
#include <map>

namespace BaoSky::dds
{
    class IDomainParticipant;
    class IDomainParticipantListener;

    class TXDDS_API DomainParticipantFactory
    {
    public:
        static std::shared_ptr<DomainParticipantFactory> GetInstance();

        virtual ~DomainParticipantFactory();

        virtual IDomainParticipant *CreateParticipant(const DomainId &domainId, const DomainParticipantQos &qos, IDomainParticipantListener *listener = nullptr, const StatusMask &mask = ALL_STATUS);

        virtual ReturnCode DeleteParticipant(IDomainParticipant *&participant);

    protected:
        DomainParticipantFactory();

        int32_t GetParticipantId();

        bool CheckParticipantId(const int32_t &partiId);

        DomainId mDefaultDomainId;

        DomainParticipantQos mDefaultPartiQos;

        static std::mutex mMutex;

        static std::shared_ptr<DomainParticipantFactory> mInstance;

        std::map<DomainId, std::vector<IDomainParticipant *>> mParticipants;

        uint32_t mPartiId;
    };
}

#endif