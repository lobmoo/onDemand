#ifndef TXDDS_RTPS_IWRITERLISTENER_H
#define TXDDS_RTPS_IWRITERLISTENER_H

#include "txdds/RTPS/common/MatchingInfo.h"

namespace BaoSky::rtps
{
    struct CacheChange;
    class IWriter;

    class IWriterListener
    {
    public:
        virtual ~IWriterListener() {}

        /**
         * This method is called when a new Reader is matched with this Writer by the builtin protocols
         * @param writer Pointer to the RTPSWriter.
         * @param info Matching Information.
         */
        virtual void OnWriterMatched(IWriter *writer, const MatchingInfo &info)
        {
            (void)writer;
            (void)info;
        }

        virtual void OnWriterChangeReceivedByAll(IWriter *writer, CacheChange *change)
        {
            (void)writer;
            (void)change;
        }
    };
}

#endif