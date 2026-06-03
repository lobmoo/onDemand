/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-01-04 14:32:59
 * @FilePath     : /TXDDS/include/txdds/RTPS/history/CacheChange.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXDDS_RTPS_CACHECHANGE_H
#define TXDDS_RTPS_CACHECHANGE_H

#include "txdds/RTPS/common/Guid.h"
#include "txdds/RTPS/common/SequenceNumber.h"
#include "txdds/RTPS/common/InstanceHandle.h"
#include "txdds/RTPS/common/SerializedPayload.h"
#include "txdds/RTPS/common/RTPSEntityTypes.h"
#include "txdds/RTPS/message/SubMessageElements.h"
#include "txdds/RTPS/history/CacheChangeProxy.h"
namespace BaoSky::rtps
{
    class IPayloadPool;

    struct CacheChange
    {
        ChangeKind kind;

        BaoSky::rtps::Guid writerGuid;

        InstanceHandle instanceHandle;
        SequenceNumber sequenceNumber;
        SerializedPayload data_value;
        ParameterList inlineQos;
        Time sourceTimestamp;
        bool hasBeenRead;
        bool canBeRead;
        bool isFragmentData;
        CacheChangeProxy missingData;
        uint16_t maxFragmentSize;
        IPayloadPool *payloadOwner;
        CacheChange()
        {
            payloadOwner = nullptr;
            writerGuid = GUID_UNKNOWN;
            kind = ChangeKind::ALIVE;
            canBeRead = false;
            hasBeenRead = false;
            isFragmentData = false;
            maxFragmentSize = 65000;
        }
        uint32_t GetFragments()
        {
            return (data_value.length / maxFragmentSize) + ((data_value.length % maxFragmentSize) ? 1 : 0);
        }
        bool Copy(const CacheChange *ch_ptr)
        {
            kind = ch_ptr->kind;
            writerGuid = ch_ptr->writerGuid;
            instanceHandle = ch_ptr->instanceHandle;
            sequenceNumber = ch_ptr->sequenceNumber;
            sourceTimestamp = ch_ptr->sourceTimestamp;
            canBeRead = ch_ptr->canBeRead;
            hasBeenRead = ch_ptr->hasBeenRead;
            missingData = ch_ptr->missingData;
            maxFragmentSize = ch_ptr->maxFragmentSize;
            isFragmentData = ch_ptr->isFragmentData;
            inlineQos = ch_ptr->inlineQos;
            return data_value.copy(&ch_ptr->data_value, false);
        }
        void CopyNotMemcpy(const CacheChange *ch_ptr)
        {
            kind = ch_ptr->kind;
            writerGuid = ch_ptr->writerGuid;
            instanceHandle = ch_ptr->instanceHandle;
            sequenceNumber = ch_ptr->sequenceNumber;
            sourceTimestamp = ch_ptr->sourceTimestamp;
            canBeRead = ch_ptr->canBeRead;
            hasBeenRead = ch_ptr->hasBeenRead;
            missingData = ch_ptr->missingData;
            maxFragmentSize = ch_ptr->maxFragmentSize;
            isFragmentData = ch_ptr->isFragmentData;
            inlineQos = ch_ptr->inlineQos;
        }
    };
}

#endif