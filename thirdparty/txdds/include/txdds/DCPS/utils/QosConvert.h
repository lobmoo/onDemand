#ifndef TXDDS_DCPS_QOSCONVERT_H
#define TXDDS_DCPS_QOSCONVERT_H

#include "txdds/DCPS/domain/qos/DomainParticipantQos.h"
#include "txdds/DCPS/topic/qos/TopicQos.h"
#include "txdds/DCPS/publisher/qos/DataWriterQos.h"
#include "txdds/DCPS/subscriber/qos/DataReaderQos.h"
#include "txdds/RTPS/attributes/ParticipantAttributes.h"
#include "txdds/RTPS/attributes/TopicAttributes.h"
#include "txdds/RTPS/attributes/WriterAttributes.h"
#include "txdds/RTPS/attributes/HistoryAttributes.h"
// #include "txdds/RTPS/attributes/SubscriberAttributes.h"
#include <txdds/RTPS/attributes/ReaderAttributes.h>
namespace BaoSky::dds::utils
{
    void SetQosfromAttributes(BaoSky::dds::DomainParticipantQos &qos, const BaoSky::rtps::ParticipantAttributes &attr);

    void SetAttributesfromQos(BaoSky::rtps::ParticipantAttributes &attr, const BaoSky::dds::DomainParticipantQos &qos);

    void SetAttributesfromQos(BaoSky::rtps::ReaderAttributes &attr, const BaoSky::dds::DataReaderQos &qos);

    /**
     * @Description: dds DataWriterQos 转换为 rtps WriterAttributes
     * @return {*}
     */
    void SetAttributesfromQos(BaoSky::rtps::WriterAttributes &attr, const BaoSky::dds::DataWriterQos &qos);

    /**
     * @Description: dds HistoryQosPolicy 转换为 rtps WriterAttributes
     * @return {*}
     */
    void SetAttributesfromQos(BaoSky::rtps::TopicAttributes &attr, const BaoSky::dds::TopicQos &qos);

}

#endif