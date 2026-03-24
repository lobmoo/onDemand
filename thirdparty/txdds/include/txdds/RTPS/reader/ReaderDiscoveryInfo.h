#ifndef TXDDS_RTPS_READERDISCOVERYINFO_H
#define TXDDS_RTPS_READERDISCOVERYINFO_H

#include "txdds/RTPS/builtin/data/DiscoveredReaderData.h"

namespace BaoSky::rtps
{
    struct ReaderDiscoveryInfo
    {
    public:
        enum DISCOVERY_STATUS
        {
            DISCOVERED_READER,
            CHANGED_QOS_READER,
            REMOVED_READER,
            IGNORED_READER
        };

        ReaderDiscoveryInfo(const DiscoveredReaderData &data) : info(data)
        {
        }
        virtual ~ReaderDiscoveryInfo() = default;

        DISCOVERY_STATUS status;
        const DiscoveredReaderData &info;
    };
}

#endif