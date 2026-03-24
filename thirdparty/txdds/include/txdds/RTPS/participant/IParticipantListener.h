#ifndef TXDDS_RTPS_IPARTICIPANTLISTENER_H
#define TXDDS_RTPS_IPARTICIPANTLISTENER_H

#include "txdds/RTPS/participant/ParticipantDiscoveryInfo.h"
#include "txdds/RTPS/reader/ReaderDiscoveryInfo.h"
#include "txdds/RTPS/writer/WriterDiscoveryInfo.h"
#include "txdds/DCPS/dynamicType/TypeObject.h"
#include "txdds/DCPS/dynamicType/TypeIdentifier.h"
#include "txdds/DCPS/dynamicType/DynamicType.h"

namespace BaoSky::rtps
{
    class IParticipant;

    class TXDDS_API IParticipantListener
    {
    public:
        virtual ~IParticipantListener() {}

        /**
         * @Description : Participant PDP 发现时触发（RTPS）
         * @param        {Participant} *participant
         * @param        {ParticipantDiscoveryInfo} &
         * @param        {bool} &should_be_ignored
         * @return       {*}
         */
        virtual void OnParticipantDiscovery(IParticipant *participant, ParticipantDiscoveryInfo &&info, bool &should_be_ignored) = 0;

        /**
         * @Description : Reader EDP 发现时触发（RTPS）
         * @param        {Participant} *participant
         * @param        {ReaderDiscoveryInfo} &
         * @param        {bool} &should_be_ignored
         * @return       {*}
         */
        virtual void OnReaderDiscovery(IParticipant *participant, ReaderDiscoveryInfo &&info, bool &should_be_ignored) = 0;

        /**
         * @Description : Writer EDP 发现时触发（RTPS）
         * @param        {Participant} *participant
         * @param        {WriterDiscoveryInfo} &
         * @param        {bool} &should_be_ignored
         * @return       {*}
         */
        virtual void OnWriterDiscovery(IParticipant *participant, WriterDiscoveryInfo &&info, bool &should_be_ignored) = 0;

        virtual void OnTypeDiscovery(IParticipant *participant, const std::string &topicName, const BaoSky::dds::TypeIdentifier *identifier,
                                     const BaoSky::dds::TypeObject *object, std::shared_ptr<BaoSky::dds::DynamicType> dynType) = 0;
    };
}

#endif