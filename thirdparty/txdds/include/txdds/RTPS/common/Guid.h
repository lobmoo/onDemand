#ifndef TXDDS_RTPS_Guid_H
#define TXDDS_RTPS_Guid_H

#include "txdds/RTPS/common/GuidPrefix.h"
#include "txdds/RTPS/common/EntityId.h"

#include <cstdint>
#include <cstring>
#include <sstream>

namespace BaoSky::rtps
{
    // Guid
    struct Guid
    {
        GuidPrefix mGuidPrefix;
        EntityId mEntityId;
        Guid()
        {
        }
        Guid(const GuidPrefix &prefix, const EntityId &entityId) : mGuidPrefix(prefix), mEntityId(entityId)
        {
        }
        Guid(const GuidPrefix &prefix, uint32_t entityId) : mGuidPrefix(prefix), mEntityId(entityId)
        {
        }
        static Guid Unknown()
        {
            return Guid();
        }
        bool IsOnSameHost(const Guid &guid)
        {
            return mGuidPrefix.IsOnSameHost(guid.mGuidPrefix);
        }
        bool IsOnSameProcess(const Guid &guid)
        {
            return mGuidPrefix.IsOnSameProcess(guid.mGuidPrefix);
        }
        bool IsBuiltin() const
        {
            return mEntityId.mValue[3] >= 0xc0;
        }
    };
    const Guid GUID_UNKNOWN;

    inline bool operator==(const Guid &g1, const Guid &g2)
    {
        if (g1.mGuidPrefix == g2.mGuidPrefix && g1.mEntityId == g2.mEntityId)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    inline bool operator!=(const Guid &g1, const Guid &g2)
    {
        return !(g1 == g2);
    }

    inline bool operator<(const Guid &g1, const Guid &g2)
    {
        int prefix_cmp = GuidPrefix::cmp(g1.mGuidPrefix, g2.mGuidPrefix);
        if (prefix_cmp < 0)
        {
            return true;
        }
        else if (prefix_cmp > 0)
        {
            return false;
        }
        else
        {
            return g1.mEntityId < g2.mEntityId;
        }
    }

    inline std::ostream &operator<<(std::ostream &output, const Guid &guid)
    {
        if (guid != GUID_UNKNOWN)
        {
            output << guid.mGuidPrefix << "|" << guid.mEntityId;
        }
        else
        {
            output << "|Guid UNKNOWN|";
        }
        return output;
    }
}

#endif