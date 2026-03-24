#ifndef TXDDS_DCPS_STATUSCONDITION_H
#define TXDDS_DCPS_STATUSCONDITION_H

#include "txdds/DCPS/core/condition/Condition.h"
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/common/StatusKind.h"

namespace BaoSky::dds
{
    class Entity;

    class StatusCondition : public Condition
    {
    public:
        StatusCondition(Entity *entity);

        virtual ~StatusCondition();

        ReturnCode set_enabled_statuses(const StatusMask &mask);

        StatusMask get_enabled_statuses();

        Entity *get_entity();

    private:
        Entity *mEntity;
    };
}

#endif