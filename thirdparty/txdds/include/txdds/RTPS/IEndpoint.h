/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-02-06 15:01:09
 * @LastEditors: zhouyuan 563733@baosight.com
 * @LastEditTime: 2025-05-21 16:30:51
 * @FilePath     : /TXDDS/include/RTPS/IEndpoint.h
 * @Description  : 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#ifndef TXDDS_RTPS_IENDPOINT_H
#define TXDDS_RTPS_IENDPOINT_H

#include "txdds/RTPS/IEntity.h"
#include "txdds/RTPS/attributes/EndpointAttributes.h"
#include "txdds/RTPS/transport/SendResource.h"
#include "txdds/RTPS/transport/RecvResource.h"
#include "txdds/RTPS/history/IChangePool.h"
#include "txdds/RTPS/history/IPayloadPool.h"
#include <txdds/RTPS/common/LocatorList_t.h>

#include <memory>
#include <algorithm>
namespace BaoSky::rtps
{
    class IHistory;
    class IParticipant;

    class TXDDS_API IEndpoint : public IEntity
    {
    public:
        IEndpoint(const BaoSky::rtps::Guid &guid, IHistory *history, IParticipant *participant) : IEntity(guid), mHistory(history), mParticipant(participant) {}

        virtual ~IEndpoint() = default;

        virtual EndpointAttributes &GetAttributes() = 0;

        virtual IHistory *GetHistory() { return mHistory; }

        virtual bool Terminate() = 0;

        virtual bool ReleaseChange(CacheChange *change) = 0;

    protected:
        IHistory *mHistory;

        IParticipant *mParticipant;
        std::mutex mSenderListLock;
        std::mutex mReceiveListLock;
        std::list<std::tuple<Locator, SendResource *>> mSenderList;
        std::list<std::tuple<Locator, RecvResource *>> mReceiverList;

        //! Pool of cache changes.
        std::shared_ptr<IChangePool> mChangePool;
        std::shared_ptr<IPayloadPool> mPayloadPool;
    };
}

#endif