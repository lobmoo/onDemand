/** 
 * @Author       : songwenguang 563734@baosight.com
 * @Date         : 2025-07-08 15:46:39
 * @FilePath     : /TXDDS/include/txdds/RTPS/builtin/data/ReaderProxyData.h
 * @Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  : 
 */
#ifndef TXDDS_RTPS_READERPROXYDATA_H
#define TXDDS_RTPS_READERPROXYDATA_H
#include "txdds/RTPS/common/Guid.h"
#include "txdds/RTPS/common/InstanceHandle.h"
#include "txdds/RTPS/common/LocatorList_t.h"
#include "txdds/RTPS/common/RTPSEntityTypes.h"

namespace BaoSky::rtps
{
    class TXDDS_API ReaderProxyData
    {
    public:
        ReaderProxyData() = default;
        virtual ~ReaderProxyData() = default;
        /** 
         * @brief 设置reader guid 
         * @param  &guid {GUID}
         * @return {}
         */        
        void SetGuid(const BaoSky::rtps::Guid &guid);
        /** 
         * @brief 设置readerkey
         * @param  &key {GUID}
         * @return {}
         */
        void SetKey(const BaoSky::rtps::Guid &key);
        /** 
         * @brief  设置participantkey
         * @param  &key {GUID}
         * @return {}
         */
        void SetParticipantKey(const BaoSky::rtps::Guid &key);
        /** 
         * @brief 设置单播locator
         * @param  &locator {LocatorList_t}
         * @return {}
         */
        void SetUnicastLocator(const LocatorList_t &locator);
        /** 
         * @brief 设置多播locator 
         * @param  &locator {LocatorList_t}
         * @return {}
         */
        void SetMulticastLocator(const LocatorList_t &locator);
        /** 
         * @brief  设置topicName
         * @param  &topicName {string}
         * @return {}
         */
        void SetTopicName(const std::string &topicName);
        /** 
         * @brief  设置typeName
         * @param  &typeName {string}
         * @return {}
         */
        void SetTypeName(const std::string &typeName);
        /** 
         * @brief  设置topickind
         * @param  &topicKind {TopicKind_t}
         * @return {}
         */
        void SetTopicKind(const TopicKind_t &topicKind);
        /** 
         * @brief  设置ExpectsInlineQos
         * @param  expectsInlineQos {bool}
         * @return {}
         */
        void SetExpectsInlineQos(bool expectsInlineQos);
        /** 
         * @brief  设置GroupGuid
         * @param  &guid {GUID}
         * @return {}
         */
        void SetGroupGuid(const BaoSky::rtps::Guid &guid);
        /** 
         * @brief 设置Reliable属性标志为true 
         * @return {}
         */
        void SetReliable();
        /** 
         * @brief 获取topicName 
         * @return {}
         */
        std::string GetTopicName();
        /** 
         * @brief 获取typeName
         * @return {}
         */
        std::string GetTypeName();
        /** 
         * @brief 获取reader guid  
         * @return {}
         */
        BaoSky::rtps::Guid &GetGuid();
        /** 
         * @brief 获取单播locator 
         * @return {}
         */
        LocatorList_t &GetUnicastLocator();
        /** 
         * @brief 获取多播locator  
         * @return {}
         */
        LocatorList_t &GetMulticastLocator();
        /** 
         * @brief 获取topickind 
         * @return {}
         */
        TopicKind_t GetTopicKind();
        /** 
         * @brief 获取participantkey 
         * @return {}
         */
        InstanceHandle GetParticipantKey();
        /** 
         * @brief 获取readerkey 
         * @return {}
         */
        InstanceHandle GetReaderKey();
        /** 
         * @brief  获取ExpectsInlineQos值
         * @return {}
         */
        bool GetExpectsInlineQos();
        /** 
         * @brief  获取Reliable属性
         * @return {}
         */
        bool GetReliable();

    private:
        BaoSky::rtps::Guid mRemoteReaderGuid;
        BaoSky::rtps::Guid mRemoteGroupGuid;
        LocatorList_t unicastLocatorList;
        LocatorList_t multicastLocatorList;
        std::string mTopicName;
        std::string mTypeName;
        TopicKind_t mTopicKind;
        InstanceHandle mReaderKey;
        InstanceHandle mParticipantKey;
        bool mExpectsInlineQos;
        bool mIsReliable = false;
    };
}

#endif