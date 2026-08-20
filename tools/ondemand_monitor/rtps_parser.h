#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>

namespace ondemand_monitor {

// RTPS Protocol Version
struct ProtocolVersion {
    uint8_t major;
    uint8_t minor;
};

// RTPS Vendor ID
struct VendorId {
    uint8_t vendorId[2];
};

// RTPS GUID
struct GUID_t {
    std::array<uint8_t, 12> prefix;  // GuidPrefix_t
    std::array<uint8_t, 4> entityId;  // EntityId_t

    bool operator==(const GUID_t& other) const {
        return prefix == other.prefix && entityId == other.entityId;
    }
};

struct GUIDHash {
    size_t operator()(const GUID_t& guid) const {
        size_t h = 0;
        for (auto b : guid.prefix) h ^= b + 0x9e3779b9 + (h << 6) + (h >> 2);
        for (auto b : guid.entityId) h ^= b + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// RTPS Locator
struct Locator_t {
    int32_t kind;
    uint32_t port;
    std::array<uint8_t, 16> address;
};

// RTPS SequenceNumber
struct SequenceNumber_t {
    int32_t high;
    uint32_t low;

    bool operator<(const SequenceNumber_t& other) const {
        if (high != other.high) return high < other.high;
        return low < other.low;
    }

    uint64_t to_u64() const {
        return (static_cast<uint64_t>(high) << 32) | low;
    }
};

// Submessage ID
enum SubmessageId : uint8_t {
    DATA = 0x15,
    DATAFRAG = 0x16,
    HEARTBEAT = 0x07,
    ACKNACK = 0x06,
    GAP = 0x08,
    INFO_TS = 0x09,
    INFO_DST = 0x0E,
    INFO_SRC = 0x0C,
    NACK_FRAG = 0x12,
    HEARTBEAT_FRAG = 0x13,
    PAD = 0x01
};

// Parsed RTPS Message
struct RtpsMessage {
    std::array<uint8_t, 12> source_guid_prefix;
    ProtocolVersion version;
    VendorId vendor_id;
};

// Parsed DATA submessage
struct DataSubmessage {
    GUID_t writer_guid;
    GUID_t reader_guid;  // ENTITYID_UNKNOWN if unicast
    SequenceNumber_t seq_num;
    uint32_t payload_size;
    const uint8_t* inline_qos;  // nullable
    uint16_t inline_qos_len;
    const uint8_t* serialized_payload;  // nullable

    // Parsed QoS parameters (extracted from inline QoS or payload)
    uint32_t domain_id = 0;
    std::string participant_name;
    std::string topic_name;
    std::string type_name;
    GUID_t endpoint_guid;  // PID_ENDPOINT_GUID (0x005A) - the actual endpoint described in SEDP
    GUID_t participant_guid;  // PID_PARTICIPANT_GUID (0x0050) - the participant being described
    bool has_domain_id = false;
    bool has_participant_name = false;
    bool has_topic_name = false;
    bool has_type_name = false;
    bool has_endpoint_guid = false;
    bool has_participant_guid = false;

    // UDP port info (from pcap header, used for participant identification)
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
};

// Parsed HEARTBEAT submessage
struct HeartbeatSubmessage {
    GUID_t writer_guid;
    GUID_t reader_guid;
    SequenceNumber_t first_sn;
    SequenceNumber_t last_sn;
    bool final_flag;
};

// Parsed ACKNACK submessage
struct AcknackSubmessage {
    GUID_t writer_guid;
    GUID_t reader_guid;
    SequenceNumber_t reader_sn_state_base;
    std::vector<uint32_t> reader_sn_state_bitmap;  // Simplified: store as vector
    bool final_flag;
};

class RtpsParser {
public:
    // Parse RTPS message header
    static bool ParseHeader(const uint8_t* data, uint32_t len, RtpsMessage& out);

    // Parse submessages (call callback for each)
    using DataCallback = void (*)(void* user, DataSubmessage&);
    using HeartbeatCallback = void (*)(void* user, const HeartbeatSubmessage&);
    using AcknackCallback = void (*)(void* user, const AcknackSubmessage&);

    static void ParseSubmessages(
        const uint8_t* data, uint32_t len,
        const std::array<uint8_t, 12>& src_guid_prefix,
        void* user,
        DataCallback on_data,
        HeartbeatCallback on_heartbeat,
        AcknackCallback on_acknack);

private:
    static bool ValidateHeader(const uint8_t* data, uint32_t len);
    static const uint8_t* ParseDataSubmessage(const uint8_t* sm, uint32_t len,
                                                const std::array<uint8_t, 12>& src_prefix,
                                                DataSubmessage& out);
    static const uint8_t* ParseHeartbeatSubmessage(const uint8_t* sm, uint32_t len,
                                                     const std::array<uint8_t, 12>& src_prefix,
                                                     HeartbeatSubmessage& out);
    static const uint8_t* ParseAcknackSubmessage(const uint8_t* sm, uint32_t len,
                                                   const std::array<uint8_t, 12>& src_prefix,
                                                   AcknackSubmessage& out);
};

}  // namespace ondemand_monitor
