#ifndef TXDDS_RTPS_PARTICIPANTDISCOVERYINFO_H
#define TXDDS_RTPS_PARTICIPANTDISCOVERYINFO_H

#include "txdds/RTPS/builtin/data/DiscoveredParticipantData.h"
#include "txdds/RTPS/builtin/data/WriterProxyData.h"
#include "txdds/RTPS/builtin/data/ReaderProxyData.h"

namespace BaoSky::rtps
{
    struct ParticipantDiscoveryInfo
    {
        enum DISCOVERY_STATUS
        {
            DISCOVERED_PARTICIPANT,
            CHANGED_QOS_PARTICIPANT,
            REMOVED_PARTICIPANT,
            DROPPED_PARTICIPANT,
            IGNORED_PARTICIPANT
        };

        ParticipantDiscoveryInfo(
            const DiscoveredParticipantData &data)
            : status(DISCOVERED_PARTICIPANT), info(data)
        {
        }

        virtual ~ParticipantDiscoveryInfo()
        {
        }

        DISCOVERY_STATUS status;

        const DiscoveredParticipantData &info;
    };
}

#endif