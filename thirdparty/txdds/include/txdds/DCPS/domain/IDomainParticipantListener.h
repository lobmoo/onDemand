#ifndef TXDDS_DCPS_IDOMAINPARTICIPANTLISTENER_H
#define TXDDS_DCPS_IDOMAINPARTICIPANTLISTENER_H

#include "txdds/DCPS/publisher/IPublisherListener.h"
#include "txdds/DCPS/topic/ITopicListener.h"
#include "txdds/DCPS/subscriber/ISubscriberListener.h"
#include "txdds/RTPS/participant/ParticipantDiscoveryInfo.h"
#include "txdds/RTPS/reader/ReaderDiscoveryInfo.h"
#include "txdds/RTPS/writer/WriterDiscoveryInfo.h"
#include "txdds/DCPS/dynamicType/TypeObject.h"
#include "txdds/DCPS/dynamicType/TypeIdentifier.h"
#include "txdds/DCPS/dynamicType/DynamicType.h"

namespace BaoSky::dds
{
    class IDomainParticipant;

    class TXDDS_API IDomainParticipantListener : public IPublisherListener, public ITopicListener, public ISubscriberListener
    {
    public:
        virtual ~IDomainParticipantListener() override {}

        virtual void OnParticipantDiscovery(IDomainParticipant *participant, BaoSky::rtps::ParticipantDiscoveryInfo &&info)
        {
            (void)participant;
            (void)info;
        }

        virtual void OnReaderDiscovery(IDomainParticipant *participant, BaoSky::rtps::ReaderDiscoveryInfo &&info)
        {
            (void)participant;
            (void)info;
        }

        virtual void OnWriterDiscovery(IDomainParticipant *participant, BaoSky::rtps::WriterDiscoveryInfo &&info)
        {
            (void)participant;
            (void)info;
        }

        virtual void OnTypeDiscovery(IDomainParticipant *participant, const std::string &topicName, const BaoSky::dds::TypeIdentifier *identifier, const BaoSky::dds::TypeObject *object, std::shared_ptr<BaoSky::dds::DynamicType> dynType)
        {
            (void)participant;
            (void)topicName;
            (void)identifier;
            (void)object;
            (void)dynType;
        }
    };
}

#endif