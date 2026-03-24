#ifndef TXDDS_DCPS_IDOMAINPARTICIPANT_H
#define TXDDS_DCPS_IDOMAINPARTICIPANT_H

#include "txdds/DCPS/core/IEntity.h"
#include "txdds/DCPS/common/DomainId.h"
#include "txdds/DCPS/domain/qos/DomainParticipantQos.h"
#include "txdds/DCPS/topic/qos/TopicQos.h"
#include "txdds/DCPS/topic/TypeSupport.h"
#include "txdds/DCPS/publisher/qos/PublisherQos.h"
#include "txdds/DCPS/subscriber/qos/SubscriberQos.h"

namespace BaoSky::rtps
{
    class IParticipant;
}

namespace BaoSky::dds
{
    class IDomainParticipantListener;
    class ITopic;
    class ITopicListener;
    class IPublisher;
    class IPublisherListener;
    class ISubscriber;
    class ISubscriberListener;

    class TXDDS_API IDomainParticipant : public IEntity
    {
    public:
        IDomainParticipant() : IEntity() {}

        virtual ~IDomainParticipant() override = default;

        virtual ReturnCode Enable() = 0;

        virtual ReturnCode Disable() = 0;

        virtual ReturnCode SetListener(IDomainParticipantListener *listener, const StatusMask &mask = ALL_STATUS) = 0;

        virtual IDomainParticipantListener *GetListener() = 0;

        virtual ReturnCode SetQos(const DomainParticipantQos &qos) = 0;

        virtual ReturnCode GetQos(DomainParticipantQos &qos) = 0;

        virtual IPublisher *CreatePublisher(const PublisherQos &qos, IPublisherListener *listener = nullptr, const StatusMask &mask = ALL_STATUS) = 0;

        virtual ReturnCode DeletePublisher(IPublisher *publisher) = 0;

        virtual ISubscriber *CreateSubscriber(const SubscriberQos &qos, ISubscriberListener *listener = nullptr, const StatusMask &mask = ALL_STATUS) = 0;

        virtual ReturnCode DeleteSubscriber(ISubscriber *subscriber) = 0;

        virtual ReturnCode RegisterType(TypeSupport type) = 0;

        virtual ReturnCode UnregisterType(TypeSupport type) = 0;

        virtual ReturnCode UnregisterType(const std::string &typeName) = 0;

        virtual TypeSupport FindType(const std::string &typeName) = 0;

        virtual ITopic *CreateTopic(const std::string &topicName, const std::string &typeName, const TopicQos &qos, ITopicListener *listener = nullptr, const StatusMask &mask = ALL_STATUS) = 0;

        virtual ReturnCode DeleteTopic(ITopic *topic) = 0;

        virtual ITopic *FindTopic(const std::string &topicName) = 0;

        virtual bool HasActiveEntities() = 0;

        virtual DomainId GetDomainId() = 0;

        virtual int32_t GetParticipantId() = 0;

        virtual BaoSky::rtps::IParticipant *GetRTPSParticipant() = 0;

        virtual ReturnCode StopAnnouncement() = 0;
        virtual ReturnCode ResetAnnouncement() = 0;
        virtual ReturnCode EnableDiscovery() = 0;
    };
}

#endif