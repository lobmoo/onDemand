/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-17 16:12:31
 * @FilePath: /TXDDS/include/RTPS/writer/WriterDiscoveryInfo.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_WRITERDISCOVERYINFO_H
#define TXDDS_RTPS_WRITERDISCOVERYINFO_H

#include "txdds/RTPS/builtin/data/DiscoveredWriterData.h"

namespace BaoSky::rtps
{
    struct WriterDiscoveryInfo
    {
    public:
        enum DISCOVERY_STATUS
        {
            DISCOVERED_WRITER,
            CHANGED_QOS_WRITER,
            REMOVED_WRITER,
            IGNORED_WRITER
        };

        WriterDiscoveryInfo(const DiscoveredWriterData &data) : info(data)
        {
        }
        virtual ~WriterDiscoveryInfo() = default;

        DISCOVERY_STATUS status;
        const DiscoveredWriterData &info;
    };
}

#endif