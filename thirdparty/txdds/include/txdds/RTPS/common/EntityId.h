/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-18 16:10:20
 * @FilePath: /TXDDS/include/RTPS/common/EntityId.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_RTPS_EntityId_H
#define TXDDS_RTPS_EntityId_H

#include <cstring>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <txdds/RTPS/common/RTPSMessageTypes.h>
namespace BaoSky::rtps
{
    // EntityId
    struct EntityId
    {
        static constexpr unsigned int mLength = 4;
        unsigned char mValue[mLength];
        EntityId()
        {
            *this = 0x00000000;
        }
        EntityId(uint32_t entityId)
        {
            memcpy(mValue, &entityId, mLength);
            if (DEFAULT_ENDIAN == LITTLEEND)
                reverse();
        }
        EntityId(const EntityId &entityId)
        {
            memcpy(mValue, entityId.mValue, mLength);
        }
        EntityId &operator=(uint32_t entityId)
        {
            memcpy(mValue, &entityId, mLength);
            if (DEFAULT_ENDIAN == LITTLEEND)
                reverse();
            return *this;
        }
        EntityId &operator=(const EntityId &entityId)
        {
            memcpy(mValue, entityId.mValue, mLength);
            return *this;
        }

        bool operator==(const EntityId &entityId) const
        {
            return memcmp(mValue, entityId.mValue, mLength) == 0;
        }
        bool operator!=(const EntityId &entityId) const
        {
            return memcmp(mValue, entityId.mValue, mLength) != 0;
        }
        bool operator<(const EntityId &entityId) const
        {
            return memcmp(mValue, entityId.mValue, mLength) < 0;
        }

        static EntityId Unknown()
        {
            return EntityId();
        }
        bool IsReader() const
        {
            return 0x4u & (*reinterpret_cast<const uint32_t *>(mValue));
        }
        bool IsWriter() const
        {
            return 0x2u & (*reinterpret_cast<const uint32_t *>(mValue)) && !IsReader();
        }

        void reverse()
        {
            octet oaux;
            oaux = mValue[3];
            mValue[3] = mValue[0];
            mValue[0] = oaux;
            oaux = mValue[2];
            mValue[2] = mValue[1];
            mValue[1] = oaux;
        }
    };
    // todo:EntityIdL_SVC
    // todo:const EntityId HAVE_SECURITY

    const EntityId ENTITYID_UNKNOWN = 0x00000000;
    const EntityId ENTITYID_RTPSParticipant = 0x000001c1;

    const EntityId ENTITYID_SEDP_BUILTIN_TOPIC_WRITER = 0x000002c2;
    const EntityId ENTITYID_SEDP_BUILTIN_TOPIC_READER = 0x000002c7;

    const EntityId ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER = 0x000003c2;
    const EntityId ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER = 0x000003c7;

    const EntityId ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER = 0x000004c2;
    const EntityId ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER = 0x000004c7;

    const EntityId ENTITYID_SPDP_BUILTIN_RTPSParticipant_WRITER = 0x000100c2;
    const EntityId ENTITYID_SPDP_BUILTIN_RTPSParticipant_READER = 0x000100c7;

    const EntityId ENTITYID_P2P_BUILTIN_RTPSParticipant_MESSAGE_WRITER = 0x000200C2;
    const EntityId ENTITYID_P2P_BUILTIN_RTPSParticipant_MESSAGE_READER = 0x000200C7;

    const EntityId ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_WRITER = 0x000201C3;
    const EntityId ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_READER = 0x000201C4;

    inline EntityId GetBuiltInWriterId(const EntityId &reader)
    {
        return (reader == ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER)     ? ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER
               : (reader == ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER)    ? ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER
               : (reader == ENTITYID_SEDP_BUILTIN_TOPIC_READER)           ? ENTITYID_SEDP_BUILTIN_TOPIC_WRITER
               : (reader == ENTITYID_SPDP_BUILTIN_RTPSParticipant_READER) ? ENTITYID_SPDP_BUILTIN_RTPSParticipant_WRITER
                                                                          : ENTITYID_UNKNOWN;
    }
    inline bool IsBuiltInEntity(const EntityId &id)
    {
        return (id == ENTITYID_RTPSParticipant) ||
               (id == ENTITYID_SEDP_BUILTIN_TOPIC_WRITER) ||
               (id == ENTITYID_SEDP_BUILTIN_TOPIC_READER) ||
               (id == ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER) ||
               (id == ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER) ||
               (id == ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER) ||
               (id == ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER) ||
               (id == ENTITYID_SPDP_BUILTIN_RTPSParticipant_WRITER) ||
               (id == ENTITYID_SPDP_BUILTIN_RTPSParticipant_READER) ||
               (id == ENTITYID_P2P_BUILTIN_RTPSParticipant_MESSAGE_WRITER) ||
               (id == ENTITYID_P2P_BUILTIN_RTPSParticipant_MESSAGE_READER) ||
               (id == ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_WRITER) ||
               (id == ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_READER);
    }
    inline std::ostream &operator<<(std::ostream &output, const EntityId &enI)
    {
        std::stringstream ss;
        ss << std::hex;
        ss << (int)enI.mValue[0] << "." << (int)enI.mValue[1] << "." << (int)enI.mValue[2] << "." << (int)enI.mValue[3];
        ss << std::dec;
        return output << ss.str();
    }
    using EntityName_t = std::string;
}

namespace std
{
    template <>
    struct hash<BaoSky::rtps::EntityId>
    {
        std::size_t operator()(const BaoSky::rtps::EntityId &k) const
        {
            return (static_cast<size_t>(k.mValue[0]) << 16) |
                   (static_cast<size_t>(k.mValue[1]) << 8) |
                   static_cast<size_t>(k.mValue[2]);
        }
    };

}
#endif