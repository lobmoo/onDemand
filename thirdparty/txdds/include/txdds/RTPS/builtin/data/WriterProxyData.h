/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-16 14:01:36
 * @FilePath: /TXDDS/include/RTPS/builtin/data/WriterProxyData.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_WRITERPROXYDATA_H
#define TXDDS_RTPS_WRITERPROXYDATA_H
#include "txdds/RTPS/common/LocatorList_t.h"
#include "txdds/RTPS/common/InstanceHandle.h"
#include "txdds/RTPS/common/RTPSEntityTypes.h"

#include <string>

namespace BaoSky::rtps
{
    class TXDDS_API WriterProxyData
    {
    public:
        WriterProxyData() = default;
        virtual ~WriterProxyData() = default;
        inline void SetGuid(const BaoSky::rtps::Guid &guid)
        {
            mRemoteWriterGuid = guid;
        }
        inline void SetKey(const BaoSky::rtps::Guid &key)
        {
            mWriterKey = key;
        }
        inline void SetParticipantKey(const BaoSky::rtps::Guid &key)
        {
            mParticipantKey = key;
        }
        inline void SetUnicastLocator(const LocatorList_t &locator)
        {
            this->unicastLocatorList.push_back(locator);
        }
        inline void SetMulticastLocator(const LocatorList_t &locator)
        {
            this->multicastLocatorList.push_back(locator);
        }
        inline void SetTopicName(const std::string &topicName)
        {
            this->mTopicName = topicName;
        }
        inline void SetTypeName(const std::string &typeName)
        {
            this->mTypeName = typeName;
        }
        inline void SetTopicKind(const TopicKind_t &topicKind)
        {
            this->mTopicKind = topicKind;
        }
        inline void SetMaxSerializeSize(long maxSerializeSize)
        {
            this->mDataMaxSizeSerialized = maxSerializeSize;
        }
        inline void SetReliable()
        {
            mIsReliable = true;
        }

        inline std::string GetTopicName() const
        {
            return this->mTopicName;
        }
        inline std::string GetTypeName() const
        {
            return this->mTypeName;
        }
        inline BaoSky::rtps::Guid &GetGuid()
        {
            return this->mRemoteWriterGuid;
        }
        inline LocatorList_t &GetUnicastLocator()
        {
            return this->unicastLocatorList;
        }
        inline LocatorList_t &GetMulticastLocator()
        {
            return this->multicastLocatorList;
        }
        inline long GetDataMaxSize()
        {
            return this->mDataMaxSizeSerialized;
        }
        inline TopicKind_t GetTopicKind()
        {
            return this->mTopicKind;
        }
        inline InstanceHandle GetParticipantKey()
        {
            return this->mParticipantKey;
        }
        inline InstanceHandle GetWriterKey()
        {
            return this->mWriterKey;
        }
        BaoSky::rtps::Guid mRemoteWriterGuid;
        LocatorList_t unicastLocatorList;
        LocatorList_t multicastLocatorList;

    private:
        BaoSky::rtps::Guid mRemoteGroupGuid;
        long mDataMaxSizeSerialized;
        std::string mTopicName;
        std::string mTypeName;
        TopicKind_t mTopicKind;
        InstanceHandle mWriterKey;
        InstanceHandle mParticipantKey;
        bool mIsReliable = false;
    };
}

#endif