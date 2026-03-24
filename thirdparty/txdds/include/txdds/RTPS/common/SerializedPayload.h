#ifndef TXDDS_RTPS_SETIALIZEDPAYLOAD_T_H
#define TXDDS_RTPS_SETIALIZEDPAYLOAD_T_H

#include "txdds/RTPS/common/RTPSMessageTypes.h"
#include <txdds/RTPS/message/CDRMessageOperator.h>
#include <cstdint>
#include <cstdlib>
#include <string.h>
#include <iostream>
#include <limits>
namespace BaoSky::rtps
{
// Pre define data encapsulation schemes
#define CDR_BE 0x0000
#define CDR_LE 0x0001
#define PL_CDR_BE 0x0002
#define PL_CDR_LE 0x0003
#define CDR2_BE 0x0010
#define CDR2_LE 0x0011
#define PL_CDR2_BE 0x0012
#define PL_CDR2_LE 0x0013
#define D_CDR_BE 0X0014
#define D_CDR_LE 0X0015
#define XML 0X0004

#if BIG_ENDIAN_DEFINE
#define DEFAULT_ENCAPSULATION CDR_BE
#define PL_DEFAULT_ENCAPSULATION PL_CDR_BE
#else
#define DEFAULT_ENCAPSULATION CDR_LE
#define PL_DEFAULT_ENCAPSULATION PL_CDR_LE
#endif // BIG_ENDIAN_DEFINE

    struct SerializedPayload
    {
        //! Size in bytes of the representation header as specified in the RTPS 2.3 specification chapter 10.
        static constexpr size_t representation_header_size = 4u;

        //! Identifier of the data as suggested in the RTPS 2.5 specification chapter 10.2.
        uint16_t mIdentifier = 0;
        //! Not used in the RTPS 2.5 specification chapter 10.2.
        uint16_t mOptions = 0;
        //! Actual length of the data
        uint32_t length = 0;
        //! Pointer to the data.
        octet *data;
        //! Maximum size of the payload
        uint32_t max_size = 0;
        //! Position when reading
        uint32_t pos = 0;

        bool isStandardPayload;

        bool needFree;
        //! Default constructor
        SerializedPayload() : mIdentifier(CDR_LE), length(0), data(nullptr), max_size(0), pos(0), isStandardPayload(true), needFree(false)
        {
        }

        /**
         * @param len Maximum size of the payload
         */
        explicit SerializedPayload(uint32_t len) : SerializedPayload()
        {
            this->reserve(len);
        }

        ~SerializedPayload()
        {
            if (needFree)
            {
                empty();
            }
        }

        bool operator==(const SerializedPayload &other) const
        {
            return ((mIdentifier == other.mIdentifier) &&
                    (length == other.length) &&
                    (0 == memcmp(data, other.data, length)));
        }

        // SerializedPayload &operator=(SerializedPayload &other)
        // {
        //     copy(&other, false);
        //     return *this;
        // }

        /*!
         * Copy another structure (including allocating new space for the data.)
         * @param[in] serData Pointer to the structure to copy
         * @param with_limit if true, the function will fail when providing a payload too big
         * @return True if correct
         */
        bool copy(const SerializedPayload *serData, bool with_limit = true)
        {
            isStandardPayload = serData->isStandardPayload;
            mIdentifier = serData->mIdentifier;
            length = serData->length;
            if (serData->max_size < serData->length)
            {
                // std::cout << "error" << std::endl;
                return false;
            }
            if (serData->max_size > max_size)
            {
                if (with_limit)
                {
                    return false;
                }
                else
                {
                    this->reserve(serData->max_size);
                }
            }
            if (length == 0)
            {
                return true;
            }
            if (data)
            {
                memcpy(data, serData->data, serData->max_size);
            }
            return true;
        }

        bool copy(const CDRMessage *serData, octet identifier)
        {
            length = serData->length + 4;

            mIdentifier = identifier;
            if (length > max_size)
            {
                this->reserve(length);
            }
            if (data == nullptr)
            {
                data = (octet *)malloc(length);
                needFree = true;
            }
            octet temp = mIdentifier;
            octet encapsulation[representation_header_size] = {0x0, temp, 0x0, 0x0};
            memcpy(data, encapsulation, representation_header_size);
            memcpy(data + representation_header_size, serData->buffer, length - 4);
            return true;
        }

        /*!
         * Allocate new space for fragmented data
         * @param[in] serData Pointer to the structure to copy
         * @return True if correct
         */
        bool reserve_fragmented(SerializedPayload *serData)
        {
            length = serData->length;
            max_size = serData->length;
            mIdentifier = serData->mIdentifier;
            data = (octet *)calloc(length, sizeof(octet));
            return true;
        }

        //! Empty the payload
        void empty()
        {
            length = 0;
            mIdentifier = CDR_LE;
            max_size = 0;
            if (data != nullptr)
            {
                free(data);
            }
            data = nullptr;
        }

        void reserve(uint32_t new_size)
        {
            if (new_size <= this->max_size)
            {
                return;
            }
            if (data == nullptr)
            {
                data = (octet *)calloc(new_size, sizeof(octet));
                if (!data)
                {
                    throw std::bad_alloc();
                }
                needFree = true;
            }
            else
            {
                void *old_data = data;
                data = (octet *)realloc(data, new_size);
                if (!data)
                {
                    free(old_data);
                    throw std::bad_alloc();
                }
                needFree = true;
                memset(data + max_size, 0, (new_size - max_size) * sizeof(octet));
            }
            max_size = new_size;
        }

        bool addFragmentData(uint32_t fragmentNumber, uint16_t fragmentSize, octet *buffer)
        {
            uint32_t datalen = fragmentSize;
            if (fragmentNumber * fragmentSize > max_size)
            {
                if (max_size < (fragmentNumber - 1) * fragmentSize)
                {
                    return false;
                }
                datalen = max_size - (fragmentNumber - 1) * fragmentSize;
            }
            length += datalen;
            memcpy(data + (fragmentNumber - 1) * fragmentSize, buffer, datalen);
            return true;
        }
    };
}

#endif