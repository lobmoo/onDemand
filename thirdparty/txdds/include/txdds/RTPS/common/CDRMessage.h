/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-22 10:59:32
 * @FilePath: /TXDDS/include/RTPS/common/CDRMessage.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */

#ifndef TXDDS_RTPS_CDRMESSAGE_H
#define TXDDS_RTPS_CDRMESSAGE_H
#include <txdds/RTPS/common/RTPSMessageTypes.h>
#include <cstdlib>
#include <cstring>

namespace BaoSky::rtps
{
#define RTPSMESSAGE_DEFAULT_SIZE 65536             // max size of rtps message in bytes
#define RTPSMESSAGE_COMMON_RTPS_PAYLOAD_SIZE 536   // common payload a rtps message has TODO(Ricardo) It is necessary?
#define RTPSMESSAGE_COMMON_DATA_PAYLOAD_SIZE 10000 // common data size
#define RTPSMESSAGE_HEADER_SIZE 20                 // header size in bytes
#define RTPSMESSAGE_SUBMESSAGEHEADER_SIZE 4
#define RTPSMESSAGE_DATA_EXTRA_INLINEQOS_SIZE 4
#define RTPSMESSAGE_INFOTS_SIZE 12

#define RTPSMESSAGE_OCTETSTOINLINEQOS_DATASUBMSG 16     // may change in future versions
#define RTPSMESSAGE_OCTETSTOINLINEQOS_DATAFRAGSUBMSG 28 // may change in future versions
#define RTPSMESSAGE_DATA_MIN_LENGTH 24
    struct CDRMessage
    {
        octet *buffer;
        uint32_t pos;
        uint32_t max_size;
        uint32_t reserved_size;
        uint32_t length;
        Endianness msg_endian;
        CDRMessage() : CDRMessage(RTPSMESSAGE_DEFAULT_SIZE)
        {
        }

        ~CDRMessage()
        {
            if (buffer != nullptr)
            {
                free(buffer);
            }
        }
        void reset(octet *input, uint32_t size)
        {
            if (buffer)
            {
                free(buffer);
                buffer = nullptr;
            }
            if (size != 0)
            {
                buffer = (octet *)malloc(size);
                memcpy(buffer, input, size);
                pos = 0;
                length = size;
            }
            else
            {
                buffer = nullptr;
            }
        }

        CDRMessage(uint32_t size)
        {
            pos = 0;
            length = 0;

            if (size != 0)
            {
                buffer = (octet *)malloc(size);
                memset(buffer, '\0', size);
            }
            else
            {
                buffer = nullptr;
            }

            max_size = size;
            reserved_size = size;
            msg_endian = DEFAULT_ENDIAN;
        }

        void move(CDRMessage &message)
        {
            pos = message.pos;
            message.pos = 0;
            length = message.length;
            message.length = 0;
            max_size = message.max_size;
            message.max_size = 0;
            reserved_size = message.reserved_size;
            message.reserved_size = 0;
            msg_endian = message.msg_endian;
            message.msg_endian = DEFAULT_ENDIAN;
            buffer = message.buffer;
            message.buffer = nullptr;
        }

        bool HasSpace(uint32_t size)
        {
            return buffer && (pos + size <= max_size);
        }

        void Resize(uint32_t size)
        {
            pos = 0;
            length = 0;
            if (buffer)
            {
                free(buffer);
                buffer = nullptr;
            }
            if (size != 0)
            {
                buffer = (octet *)malloc(size);
                memset(buffer, '\0', size);
            }
            else
            {
                buffer = nullptr;
            }
            max_size = size;
            reserved_size = size;
            msg_endian = DEFAULT_ENDIAN;
        }
    };
}

#endif