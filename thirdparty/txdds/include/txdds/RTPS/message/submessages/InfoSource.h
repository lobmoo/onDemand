/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-19 16:46:34
 * @FilePath    : /TXDDS/include/RTPS/message/submessages/InfoSource.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_INFOSOURCEMSG_H_
#define _TXDDS_RTPS_INFOSOURCEMSG_H_
#include <txdds/RTPS/message/SubMessageHeader.h>
namespace BaoSky::rtps
{
    struct InfoSourceMsg
    {
        ProtocolVersion protocolVersion;
        VendorId vendorId;
        GuidPrefix guidPrefix;
    };

}
#endif