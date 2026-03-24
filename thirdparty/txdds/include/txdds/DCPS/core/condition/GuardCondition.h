#ifndef TXDDS_DCPS_GUARDCONDITION_H
#define TXDDS_DCPS_GUARDCONDITION_H

#include "txdds/DCPS/core/condition/Condition.h"
#include "txdds/DCPS/common/ReturnCode.h"

namespace BaoSky::dds
{
    class GuardCondition : public Condition
    {
    public:
        ReturnCode set_trigger_value(const bool &value);
    };
}

#endif