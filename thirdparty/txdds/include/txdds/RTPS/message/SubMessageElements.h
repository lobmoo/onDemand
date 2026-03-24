/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-11-25 10:12:53
 * @FilePath     : /TXDDS/include/txdds/RTPS/message/SubMessageElements.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */

#ifndef _TXDDS_RTPS_SUBMESSAGEELEMENTS_H_
#define _TXDDS_RTPS_SUBMESSAGEELEMENTS_H_
#include <txdds/RTPS/common/ChangeCount_t.h>
#include <txdds/RTPS/common/EntityId.h>
#include <txdds/RTPS/common/GuidPrefix.h>
#include <txdds/RTPS/common/LocatorList_t.h>
#include <txdds/RTPS/common/RTPSEntityTypes.h>
#include <txdds/RTPS/common/SequenceNumber.h>
#include "txdds/RTPS/common/Parameter.h"

#include <txdds/RTPS/common/Guid.h>
#include <txdds/RTPS/common/Time.h>
#include <txdds/RTPS/common/FragmentNumber_t.h>
namespace BaoSky::rtps
{
    struct SerializedPayload;
    typedef ChangeCount_t ChangeCount;
    typedef FragmentNumber_t FragmentNumber;
    typedef FragmentNumberSet_t FragmentNumberSet;
    typedef LocatorList_t LocatorList;
    typedef uint32_t MessageLength;
    typedef std::vector<std::shared_ptr<Parameter>> ParameterList;
    typedef ProtocolVersion_t ProtocolVersion;
    typedef SerializedPayload SerializedData;
    typedef Time Timestamp;
    typedef VendorId_t VendorId;
}
#endif