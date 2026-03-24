/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-05-07 14:19:06
 * @FilePath: /PF/home/bx/program/TXDDS/include/DCPS/dynamicType/DynamicTypeParameter.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */

#ifndef TXDDS_DCPS_DynamicTypeParameter_H
#define TXDDS_DCPS_DynamicTypeParameter_H
#include "txdds/RTPS/common/Parameter.h"
#include "txdds/RTPS/common/RTPSMessageTypes.h"
#include "txdds/DCPS/core/policy/QosPolicy.h"
#include "txdds/DCPS/dynamicType/TypeObject.h"
#include "txdds/DCPS/dynamicType/TypeIdentifier.h"
#include "txdds/RTPS/common/SerializedPayload.h"
#include "txcdr/CdrSizeCalculator.h"
#include "txcdr/Cdr.h"
#include "txcdr/TXBuffer.h"
#include "txcdr/SerializeCdr.h"
#include "txcdr/DeserializeCdr.h"
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>
using namespace BaoSky::Cdr;
using namespace BaoSky::rtps;
namespace BaoSky::dds
{
    class TXDDS_API TypeIdentifierParameter : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        TypeIdentifier mTypeIdentifier;
        TypeIdentifierParameter() : BaoSky::rtps::Parameter(BaoSky::rtps::ParameterId::PID_TYPE_IDV1, 0), QosPolicy(false), mTypeIdentifier()
        {
        }
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength);

        static std::shared_ptr<BaoSky::rtps::Parameter> CreateInstance()
        {
            return std::static_pointer_cast<Parameter>(std::make_shared<TypeIdentifierParameter>());
        }
    };

    class TXDDS_API TypeObjectParameter : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        TypeObject mTypeobject;
        TypeObjectParameter() : BaoSky::rtps::Parameter(BaoSky::rtps::ParameterId::PID_TYPE_OBJECTV1, 0), QosPolicy(false), mTypeobject()
        {
        }
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;

        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength);

        static std::shared_ptr<BaoSky::rtps::Parameter> CreateInstance()
        {
            return std::static_pointer_cast<Parameter>(std::make_shared<TypeObjectParameter>());
        }
    };
}
#endif