#ifndef TXDDS_DCPS_CONDITION_H
#define TXDDS_DCPS_CONDITION_H

namespace BaoSky::dds
{
    class Condition
    {
        virtual bool get_trigger_value();
    };
}

#endif