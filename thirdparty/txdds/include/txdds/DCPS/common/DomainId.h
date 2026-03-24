#ifndef TXDDS_DCPS_DOMAINID_H
#define TXDDS_DCPS_DOMAINID_H

#include <cstdint>

namespace BaoSky::dds
{
    typedef uint32_t DomainId;

    const DomainId DOMAIN_ID_UNKNOWN = 0xFFFFFFFF;

    const int32_t LENGTH_UNLIMITED = -1;
}

#endif