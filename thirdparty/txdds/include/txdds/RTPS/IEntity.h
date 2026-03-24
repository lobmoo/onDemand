#ifndef TXDDS_RTPS_IENTITY_H
#define TXDDS_RTPS_IENTITY_H

#include "txdds/RTPS/common/Guid.h"
#include "txdds/txddsexport.h"

#include <mutex>
#include <atomic>

namespace BaoSky::rtps
{
    class TXDDS_API IEntity
    {
    public:
        IEntity(const BaoSky::rtps::Guid &guid) : mGUID(guid), mIsInitial(false) {}

        virtual ~IEntity() {}

        virtual bool Initial() = 0;

        virtual bool Terminate() = 0;

        virtual BaoSky::rtps::Guid GetGuid() { return mGUID; }

        virtual std::mutex &GetMutex() { return mMtx; }

    protected:
        BaoSky::rtps::Guid mGUID;

        std::atomic_bool mIsInitial;

        std::mutex mMtx;
    };
}

#endif