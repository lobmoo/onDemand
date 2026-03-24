#ifndef TXDDS_DCPS_WAITSET_H
#define TXDDS_DCPS_WAITSET_H

#include "txdds/DCPS/core/condition/Condition.h"
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/common/Duration.h"

namespace BaoSky::dds
{
    class WaitSet
    {
        WaitSet() = default;

        virtual WaitSet() = default;

        ReturnCode attach_condition(const Condition &a_condition);

        ReturnCode detach_condition(const Condition &a_condition);

        ReturnCode wait(std::vector<Condition> &active_conditions, const Duration &timeout);

        ReturnCode get_conditions(std::vector<Condition> &attached_conditions);
    };
}

#endif