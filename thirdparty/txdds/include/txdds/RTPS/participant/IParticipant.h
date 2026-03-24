/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-03-17 10:44:21
 * @FilePath     : /TXDDS/include/RTPS/participant/IParticipant.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXDDS_RTPS_IPARTICIPANT_H
#define TXDDS_RTPS_IPARTICIPANT_H

#include "txdds/RTPS/IEntity.h"
#include "txdds/RTPS/attributes/WriterAttributes.h"
#include "txdds/RTPS/attributes/ReaderAttributes.h"
#include "txdds/RTPS/attributes/TopicAttributes.h"
#include "txdds/RTPS/attributes/ParticipantAttributes.h"
#include "txdds/RTPS/timedEvent/TimedEventService.h"
#include "txdds/DCPS/builtin/topic/PublicationBuiltinTopicData.h"
#include "txdds/DCPS/builtin/topic/SubscriptionBuiltinTopicData.h"
#include "txdds/RTPS/history/PayloadPoolFactory.h"

namespace BaoSky::rtps
{
    class IWriterHistory;
    class IReaderHistory;
    class IEndpoint;
    class IWriter;
    class IReader;
    class IWriterListener;
    class IReaderListener;
    class IParticipantListener;

    class TXDDS_API IParticipant : public IEntity
    {
    public:
        IParticipant(const uint32_t &domainId, const BaoSky::rtps::Guid &guid) : IEntity(guid), mDomainId(domainId) {}

        virtual ~IParticipant() override {}

        virtual IWriter *CreateWriter(IWriterHistory *history, WriterAttributes &watt, const bool &isBuiltin,
                                      const std::shared_ptr<IPayloadPool> &payloadPool, const std::shared_ptr<IChangePool> &changePool,
                                      IWriterListener *listener = nullptr, const EntityId &entityId = ENTITYID_UNKNOWN) = 0;

        virtual IReader *CreateReader(IReaderHistory *history, ReaderAttributes &ratt, const bool &isBuiltin,
                                      const std::shared_ptr<IPayloadPool> &payloadPool, const std::shared_ptr<IChangePool> &changePool,
                                      IReaderListener *listener = nullptr, const EntityId &entityId = ENTITYID_UNKNOWN) = 0;

        virtual bool DeleteWriter(IWriter *writer, const bool &isBuiltin = false) = 0;

        virtual bool DeleteReader(IReader *reader, const bool &isBuiltin = false) = 0;

        virtual bool RegisterWriter(IWriter *writer, const TopicAttributes &topicAtt, const BaoSky::dds::builtin::PublicationBuiltinTopicData &wQos) = 0;

        virtual bool RegisterReader(IReader *reader, const TopicAttributes &topicAtt, const BaoSky::dds::builtin::SubscriptionBuiltinTopicData &rQos) = 0;

        virtual bool UnRegisterWriter(IWriter *writer) = 0;

        virtual bool UnRegisterReader(IReader *reader) = 0;

        virtual ParticipantAttributes GetParticipantAttributes() = 0;

        virtual IParticipantListener *GetParticipantListener() = 0;

        virtual TimedEventService &GetTimedEventSVC() = 0;

        virtual std::vector<IWriter *> GetUserWriter() = 0;

        virtual std::vector<IReader *> GetUserReader() = 0;

        virtual bool UpdateRemoteParticipantLiveliness(const GuidPrefix &parGuidPrefix) = 0;

        virtual bool StopAnnouncement() = 0;

        virtual bool ResetAnnouncement() = 0;

        std::mutex &GetWriterMutex() { return mWriterMtx; }

        std::mutex &GetReaderMutex() { return mReaderMtx; }

        virtual bool EnableDiscovery() = 0;

        inline uint32_t GetDomainId() { return mDomainId; }

        virtual IWriter *FindWriterByGuid(const BaoSky::rtps::Guid &writerGuid) = 0;

        virtual IReader *FindReaderByGuid(const BaoSky::rtps::Guid &readerGuid) = 0;

    protected:
        uint32_t mDomainId;

        std::mutex mWriterMtx;

        std::mutex mReaderMtx;
    };
}

#endif