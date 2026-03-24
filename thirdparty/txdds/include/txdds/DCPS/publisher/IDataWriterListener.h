/*
 * @Author      : wanglin wanglin@baosight.com
 * @Date        : 2025-02-14 16:51:26
 * @FilePath    : /TXDDS/include/DCPS/publisher/IDataWriterListener.h
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_IDATAWRITERLISTENER_H
#define TXDDS_DCPS_IDATAWRITERLISTENER_H

#include "txdds/DCPS/core/status/PublicationMatchedStatus.h"
#include "txdds/txddsexport.h"

namespace BaoSky::dds
{
    class IDataWriter;

    class TXDDS_API IDataWriterListener
    {
    public:
        virtual ~IDataWriterListener() {}

        virtual void OnPublicationMatched(IDataWriter *writer, const PublicationMatchedStatus &status)
        {
            (void)writer;
            (void)status;
        }
    };
}

#endif