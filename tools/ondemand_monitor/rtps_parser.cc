#include "rtps_parser.h"
#include <cstring>
#include <arpa/inet.h>
#include <unordered_set>

namespace ondemand_monitor {

// RTPS magic: "RTPS"
static const uint8_t RTPS_MAGIC[4] = {'R', 'T', 'P', 'S'};

bool RtpsParser::ValidateHeader(const uint8_t* data, uint32_t len) {
    if (len < 20) return false;
    return std::memcmp(data, RTPS_MAGIC, 4) == 0;
}

bool RtpsParser::ParseHeader(const uint8_t* data, uint32_t len, RtpsMessage& out) {
    if (!ValidateHeader(data, len)) {
        return false;
    }

    out.version.major = data[4];
    out.version.minor = data[5];
    out.vendor_id.vendorId[0] = data[6];
    out.vendor_id.vendorId[1] = data[7];
    std::memcpy(out.source_guid_prefix.data(), data + 8, 12);

    return true;
}

void RtpsParser::ParseSubmessages(
    const uint8_t* data, uint32_t len,
    const std::array<uint8_t, 12>& src_guid_prefix,
    void* user,
    DataCallback on_data,
    HeartbeatCallback on_heartbeat,
    AcknackCallback on_acknack) {

    const uint8_t* ptr = data + 20;  // Skip RTPS header
    const uint8_t* end = data + len;

    while (ptr + 4 <= end) {
        uint8_t submsg_id = ptr[0];
        uint8_t flags = ptr[1];
        uint16_t octets_to_next = *reinterpret_cast<const uint16_t*>(ptr + 2);

        // Handle endianness flag (bit 0)
        bool little_endian = (flags & 0x01) != 0;
        if (!little_endian) {
            octets_to_next = ntohs(octets_to_next);
        }

        const uint8_t* submsg_end = (octets_to_next == 0) ? end : ptr + 4 + octets_to_next;
        if (submsg_end > end) {
            break;  // Malformed
        }

        switch (submsg_id) {
            case SubmessageId::DATA: {
                if (on_data) {
                    DataSubmessage ds;
                    if (ParseDataSubmessage(ptr, submsg_end - ptr, src_guid_prefix, ds)) {
                        on_data(user, ds);
                    }
                }
                break;
            }
            case SubmessageId::HEARTBEAT: {
                if (on_heartbeat) {
                    HeartbeatSubmessage hb;
                    if (ParseHeartbeatSubmessage(ptr, submsg_end - ptr, src_guid_prefix, hb)) {
                        on_heartbeat(user, hb);
                    }
                }
                break;
            }
            case SubmessageId::ACKNACK: {
                if (on_acknack) {
                    AcknackSubmessage ack;
                    if (ParseAcknackSubmessage(ptr, submsg_end - ptr, src_guid_prefix, ack)) {
                        on_acknack(user, ack);
                    }
                }
                break;
            }
            default:
                // Ignore other submessages
                break;
        }

        ptr = (octets_to_next == 0) ? end : submsg_end;
    }
}

const uint8_t* RtpsParser::ParseDataSubmessage(const uint8_t* sm, uint32_t len,
                                                 const std::array<uint8_t, 12>& src_prefix,
                                                 DataSubmessage& out) {
    if (len < 24) return nullptr;

    uint8_t flags = sm[1];
    bool little_endian = (flags & 0x01) != 0;
    const uint8_t* ptr = sm + 4;

    // extraFlags (2 bytes) + octetsToInlineQos (2 bytes)
    uint16_t octets_to_inline_qos = little_endian ?
        *reinterpret_cast<const uint16_t*>(ptr + 2) :
        ntohs(*reinterpret_cast<const uint16_t*>(ptr + 2));
    ptr += 4;

    // readerEntityId (4 bytes)
    std::memcpy(out.reader_guid.entityId.data(), ptr, 4);
    std::memcpy(out.reader_guid.prefix.data(), src_prefix.data(), 12);
    ptr += 4;

    // writerEntityId (4 bytes)
    std::memcpy(out.writer_guid.entityId.data(), ptr, 4);
    std::memcpy(out.writer_guid.prefix.data(), src_prefix.data(), 12);
    ptr += 4;

    // writerSeqNum (8 bytes)
    out.seq_num.high = little_endian ?
        *reinterpret_cast<const int32_t*>(ptr) :
        ntohl(*reinterpret_cast<const int32_t*>(ptr));
    out.seq_num.low = little_endian ?
        *reinterpret_cast<const uint32_t*>(ptr + 4) :
        ntohl(*reinterpret_cast<const uint32_t*>(ptr + 4));
    ptr += 8;

    // InlineQoS (if Q flag set)
    out.inline_qos = nullptr;
    out.inline_qos_len = 0;
    if (flags & 0x02) {
        // Calculate InlineQoS length from octetsToInlineQoS
        const uint8_t* inline_qos_start = sm + 4 + octets_to_inline_qos;
        const uint8_t* inline_qos_end = nullptr;

        // Find end of InlineQoS (before serialized payload or end of submessage)
        if (flags & 0x04) {
            // D flag set: serialized payload follows InlineQoS
            inline_qos_end = sm + len;  // Will be adjusted below
        } else {
            inline_qos_end = sm + len;
        }

        out.inline_qos = inline_qos_start;
        out.inline_qos_len = static_cast<uint16_t>(inline_qos_end - inline_qos_start);

        // Parse ParameterList to extract domain_id and participant_name
        const uint8_t* param_ptr = inline_qos_start;
        while (param_ptr + 4 <= inline_qos_end) {
            uint16_t param_id = little_endian ?
                *reinterpret_cast<const uint16_t*>(param_ptr) :
                ntohs(*reinterpret_cast<const uint16_t*>(param_ptr));
            uint16_t param_len = little_endian ?
                *reinterpret_cast<const uint16_t*>(param_ptr + 2) :
                ntohs(*reinterpret_cast<const uint16_t*>(param_ptr + 2));

            // PID_SENTINEL marks end of parameter list
            if (param_id == 0x0001) {
                break;
            }

            const uint8_t* param_data = param_ptr + 4;

            // PID_DOMAIN_ID = 0x000f (4 bytes, uint32_t)
            if (param_id == 0x000f && param_len >= 4) {
                out.domain_id = little_endian ?
                    *reinterpret_cast<const uint32_t*>(param_data) :
                    ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                out.has_domain_id = true;
            }
            // PID_PARTICIPANT_NAME = 0x0029 (string: 4 bytes length + data)
            else if (param_id == 0x0029 && param_len >= 4) {
                uint32_t str_len = little_endian ?
                    *reinterpret_cast<const uint32_t*>(param_data) :
                    ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                if (str_len > 0 && str_len <= static_cast<uint32_t>(param_len - 4)) {
                    out.participant_name = std::string(
                        reinterpret_cast<const char*>(param_data + 4), str_len);
                    // Remove null terminator if present
                    if (!out.participant_name.empty() && out.participant_name.back() == '\0') {
                        out.participant_name.pop_back();
                    }
                    out.has_participant_name = true;
                }
            }
            // PID_TOPIC_NAME = 0x0005 (string: 4 bytes length + data)
            else if (param_id == 0x0005 && param_len >= 4) {
                uint32_t str_len = little_endian ?
                    *reinterpret_cast<const uint32_t*>(param_data) :
                    ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                if (str_len > 0 && str_len <= static_cast<uint32_t>(param_len - 4)) {
                    out.topic_name = std::string(
                        reinterpret_cast<const char*>(param_data + 4), str_len);
                    // Remove null terminator if present
                    if (!out.topic_name.empty() && out.topic_name.back() == '\0') {
                        out.topic_name.pop_back();
                    }
                    out.has_topic_name = true;
                }
            }
            // PID_TYPE_NAME = 0x0007 (string: 4 bytes length + data)
            else if (param_id == 0x0007 && param_len >= 4) {
                uint32_t str_len = little_endian ?
                    *reinterpret_cast<const uint32_t*>(param_data) :
                    ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                if (str_len > 0 && str_len <= static_cast<uint32_t>(param_len - 4)) {
                    out.type_name = std::string(
                        reinterpret_cast<const char*>(param_data + 4), str_len);
                    // Remove null terminator if present
                    if (!out.type_name.empty() && out.type_name.back() == '\0') {
                        out.type_name.pop_back();
                    }
                    out.has_type_name = true;
                }
            }
            // PID_ENDPOINT_GUID = 0x005A (GUID: 12 bytes prefix + 4 bytes entityId = 16 bytes)
            else if (param_id == 0x005A && param_len >= 16) {
                std::memcpy(out.endpoint_guid.prefix.data(), param_data, 12);
                std::memcpy(out.endpoint_guid.entityId.data(), param_data + 12, 4);
                out.has_endpoint_guid = true;
            }

            // Move to next parameter (align to 4 bytes)
            param_ptr = param_data + param_len;
            // Align to 4-byte boundary
            while ((param_ptr - sm) % 4 != 0 && param_ptr < inline_qos_end) {
                param_ptr++;
            }
        }
    }

    // SerializedPayload (if D flag set) - also parse for parameters
    out.serialized_payload = nullptr;
    out.payload_size = 0;
    if (flags & 0x04) {
        out.serialized_payload = ptr;
        out.payload_size = (sm + len) - ptr;

        // Debug: dump all unique writer and reader entity IDs
        // Parse SerializedPayload for ParameterList (SPDP/SEDP discovery data)
        // SerializedPayload starts with CDR header (4 bytes): encoding (2 bytes) + options (2 bytes)
        const uint8_t* payload_ptr = ptr;
        const uint8_t* payload_end = sm + len;

        if (out.payload_size >= 8) {
            // Read CDR encoding to determine endianness
            uint16_t cdr_encoding = *reinterpret_cast<const uint16_t*>(payload_ptr);
            // CDR encoding values (in wire byte order):
            // 0x0000 = CDR Big Endian, 0x0001 = CDR Little Endian
            // 0x0002 = PL_CDR Big Endian, 0x0003 = PL_CDR Little Endian
            // When read as little-endian uint16_t:
            // 0x0001 = CDR_LE, 0x0300 = PL_CDR_LE
            bool payload_little_endian = (cdr_encoding == 0x0001 || cdr_encoding == 0x0300 || little_endian);

            // Skip CDR header (4 bytes)
            payload_ptr += 4;
            // Save parameter list start for alignment calculation
            const uint8_t* param_list_start = payload_ptr;

            // Parse ParameterList
            while (payload_ptr + 4 <= payload_end) {
                uint16_t param_id = payload_little_endian ?
                    *reinterpret_cast<const uint16_t*>(payload_ptr) :
                    ntohs(*reinterpret_cast<const uint16_t*>(payload_ptr));
                uint16_t param_len = payload_little_endian ?
                    *reinterpret_cast<const uint16_t*>(payload_ptr + 2) :
                    ntohs(*reinterpret_cast<const uint16_t*>(payload_ptr + 2));

                // PID_SENTINEL marks end of parameter list
                if (param_id == 0x0001) {
                    break;
                }

                const uint8_t* param_data = payload_ptr + 4;

                // PID_DOMAIN_ID = 0x000f (4 bytes, uint32_t)
                if (param_id == 0x000f && param_len >= 4 && !out.has_domain_id) {
                    out.domain_id = payload_little_endian ?
                        *reinterpret_cast<const uint32_t*>(param_data) :
                        ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                    out.has_domain_id = true;
                }
                // PID_PARTICIPANT_NAME = 0x0029 (string: 4 bytes length + data)
                else if (param_id == 0x0029 && param_len >= 4 && !out.has_participant_name) {
                    uint32_t str_len = payload_little_endian ?
                        *reinterpret_cast<const uint32_t*>(param_data) :
                        ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                    if (str_len > 0 && str_len <= static_cast<uint32_t>(param_len - 4)) {
                        out.participant_name = std::string(
                            reinterpret_cast<const char*>(param_data + 4), str_len);
                        // Remove null terminator if present
                        if (!out.participant_name.empty() && out.participant_name.back() == '\0') {
                            out.participant_name.pop_back();
                        }
                        out.has_participant_name = true;
                    }
                }
                // PID 0x0062 - Vendor-specific parameter that contains participant/node name
                // Format: 4 bytes string length + string data
                else if (param_id == 0x0062 && param_len >= 4 && !out.has_participant_name) {
                    uint32_t str_len = payload_little_endian ?
                        *reinterpret_cast<const uint32_t*>(param_data) :
                        ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                    if (str_len > 0 && str_len <= static_cast<uint32_t>(param_len - 4)) {
                        out.participant_name = std::string(
                            reinterpret_cast<const char*>(param_data + 4), str_len);
                        // Remove null terminator if present
                        if (!out.participant_name.empty() && out.participant_name.back() == '\0') {
                            out.participant_name.pop_back();
                        }
                        out.has_participant_name = true;
                    }
                }
                // PID_TOPIC_NAME = 0x0005 (string: 4 bytes length + data)
                else if (param_id == 0x0005 && param_len >= 4 && !out.has_topic_name) {
                    uint32_t str_len = payload_little_endian ?
                        *reinterpret_cast<const uint32_t*>(param_data) :
                        ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                    if (str_len > 0 && str_len <= static_cast<uint32_t>(param_len - 4)) {
                        out.topic_name = std::string(
                            reinterpret_cast<const char*>(param_data + 4), str_len);
                        // Remove null terminator if present
                        if (!out.topic_name.empty() && out.topic_name.back() == '\0') {
                            out.topic_name.pop_back();
                        }
                        out.has_topic_name = true;
                    }
                }
                // PID_TYPE_NAME = 0x0007 (string: 4 bytes length + data)
                else if (param_id == 0x0007 && param_len >= 4 && !out.has_type_name) {
                    uint32_t str_len = payload_little_endian ?
                        *reinterpret_cast<const uint32_t*>(param_data) :
                        ntohl(*reinterpret_cast<const uint32_t*>(param_data));
                    if (str_len > 0 && str_len <= static_cast<uint32_t>(param_len - 4)) {
                        out.type_name = std::string(
                            reinterpret_cast<const char*>(param_data + 4), str_len);
                        // Remove null terminator if present
                        if (!out.type_name.empty() && out.type_name.back() == '\0') {
                            out.type_name.pop_back();
                        }
                        out.has_type_name = true;
                    }
                }
                // PID_ENDPOINT_GUID = 0x005A (GUID: 12 bytes prefix + 4 bytes entityId = 16 bytes)
                else if (param_id == 0x005A && param_len >= 16 && !out.has_endpoint_guid) {
                    std::memcpy(out.endpoint_guid.prefix.data(), param_data, 12);
                    std::memcpy(out.endpoint_guid.entityId.data(), param_data + 12, 4);
                    out.has_endpoint_guid = true;
                }
                // PID_PARTICIPANT_GUID = 0x0050 (GUID: 12 bytes prefix + 4 bytes entityId = 16 bytes)
                else if (param_id == 0x0050 && param_len >= 16 && !out.has_participant_guid) {
                    std::memcpy(out.participant_guid.prefix.data(), param_data, 12);
                    std::memcpy(out.participant_guid.entityId.data(), param_data + 12, 4);
                    out.has_participant_guid = true;

                    // Debug: log participant GUID
                }

                // Move to next parameter (align to 4 bytes)
                payload_ptr = param_data + param_len;
                // Align to 4-byte boundary relative to parameter list start
                while ((payload_ptr - param_list_start) % 4 != 0 && payload_ptr < payload_end) {
                    payload_ptr++;
                }
            }
        }
    }


    return ptr;
}

const uint8_t* RtpsParser::ParseHeartbeatSubmessage(const uint8_t* sm, uint32_t len,
                                                      const std::array<uint8_t, 12>& src_prefix,
                                                      HeartbeatSubmessage& out) {
    if (len < 32) return nullptr;

    uint8_t flags = sm[1];
    const uint8_t* ptr = sm + 4;

    // readerEntityId (4 bytes)
    std::memcpy(out.reader_guid.entityId.data(), ptr, 4);
    std::memcpy(out.reader_guid.prefix.data(), src_prefix.data(), 12);
    ptr += 4;

    // writerEntityId (4 bytes)
    std::memcpy(out.writer_guid.entityId.data(), ptr, 4);
    std::memcpy(out.writer_guid.prefix.data(), src_prefix.data(), 12);
    ptr += 4;

    // firstSeqNumber (8 bytes)
    out.first_sn.high = *reinterpret_cast<const int32_t*>(ptr);
    out.first_sn.low = *reinterpret_cast<const uint32_t*>(ptr + 4);
    ptr += 8;

    // lastSeqNumber (8 bytes)
    out.last_sn.high = *reinterpret_cast<const int32_t*>(ptr);
    out.last_sn.low = *reinterpret_cast<const uint32_t*>(ptr + 4);
    ptr += 8;

    // count (4 bytes) - ignored
    ptr += 4;

    out.final_flag = (flags & 0x02) != 0;
    return ptr;
}

const uint8_t* RtpsParser::ParseAcknackSubmessage(const uint8_t* sm, uint32_t len,
                                                    const std::array<uint8_t, 12>& src_prefix,
                                                    AcknackSubmessage& out) {
    if (len < 24) return nullptr;

    uint8_t flags = sm[1];
    const uint8_t* ptr = sm + 4;

    // readerEntityId (4 bytes)
    std::memcpy(out.reader_guid.entityId.data(), ptr, 4);
    std::memcpy(out.reader_guid.prefix.data(), src_prefix.data(), 12);
    ptr += 4;

    // writerEntityId (4 bytes)
    std::memcpy(out.writer_guid.entityId.data(), ptr, 4);
    std::memcpy(out.writer_guid.prefix.data(), src_prefix.data(), 12);
    ptr += 4;

    // readerSNState.base (8 bytes)
    out.reader_sn_state_base.high = *reinterpret_cast<const int32_t*>(ptr);
    out.reader_sn_state_base.low = *reinterpret_cast<const uint32_t*>(ptr + 4);
    ptr += 8;

    // readerSNState (numBits + bitmap)
    if (ptr + 4 <= sm + len) {
        uint32_t num_bits = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;
        uint32_t num_words = (num_bits + 31) / 32;

        out.reader_sn_state_bitmap.clear();
        for (uint32_t i = 0; i < num_words && ptr + 4 <= sm + len; ++i) {
            out.reader_sn_state_bitmap.push_back(*reinterpret_cast<const uint32_t*>(ptr));
            ptr += 4;
        }
    }

    // count (4 bytes) - ignored

    out.final_flag = (flags & 0x02) != 0;
    return ptr;
}

}  // namespace ondemand_monitor
