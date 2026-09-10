#include "match_analyzer.h"
#include <cstring>
#include <arpa/inet.h>

namespace ondemand_monitor {

// RTPS PID definitions
enum Pid : uint16_t {
    PID_SENTINEL = 0x0001,
    PID_TOPIC_NAME = 0x0005,
    PID_TYPE_NAME = 0x0007,
    PID_RELIABILITY = 0x001A,
    PID_DURABILITY = 0x001D,
    PID_OWNERSHIP = 0x001F,
    PID_LIVELINESS = 0x001B,
    PID_DEADLINE = 0x0023,
    PID_PARTICIPANT_GUID = 0x0050,
    PID_ENDPOINT_GUID = 0x005A,
};

const uint8_t* MatchAnalyzer::FindParameter(const uint8_t* params, uint16_t len, uint16_t pid) {
    if (!params) return nullptr;

    size_t offset = 0;
    while (offset + 4 <= len) {
        const uint8_t* ptr = params + offset;
        uint16_t current_pid, param_len;
        std::memcpy(&current_pid, ptr, sizeof(current_pid));
        std::memcpy(&param_len, ptr + 2, sizeof(param_len));

        if (current_pid == Pid::PID_SENTINEL) {
            break;
        }

        if (current_pid == pid) {
            return ptr + 4;  // Return data pointer
        }

        const size_t payload_end = offset + 4 + param_len;
        if (payload_end > len) break;
        const size_t aligned_end = (payload_end + 3u) & ~size_t(3u);
        if (aligned_end > len) break;
        offset = aligned_end;
    }

    return nullptr;
}

bool MatchAnalyzer::ExtractTopicName(const uint8_t* inline_qos, uint16_t len, std::string& out) {
    const uint8_t* param = FindParameter(inline_qos, len, Pid::PID_TOPIC_NAME);
    if (!param) {
        return false;
    }

    // param points into [inline_qos, inline_qos+len); validate with sizes
    // before forming pointers from the untrusted uint32 length.
    const size_t param_offset = static_cast<size_t>(param - inline_qos);
    if (param_offset > len || len - param_offset < 4) return false;

    uint32_t str_len;
    std::memcpy(&str_len, param, sizeof(str_len));
    const size_t available = len - param_offset - 4;
    if (str_len == 0 || static_cast<size_t>(str_len) > available) {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(param + 4), str_len);
    return true;
}

bool MatchAnalyzer::ExtractTypeName(const uint8_t* inline_qos, uint16_t len, std::string& out) {
    const uint8_t* param = FindParameter(inline_qos, len, Pid::PID_TYPE_NAME);
    if (!param) {
        return false;
    }

    const size_t param_offset = static_cast<size_t>(param - inline_qos);
    if (param_offset > len || len - param_offset < 4) return false;

    uint32_t str_len;
    std::memcpy(&str_len, param, sizeof(str_len));
    const size_t available = len - param_offset - 4;
    if (str_len == 0 || static_cast<size_t>(str_len) > available) {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(param + 4), str_len);
    return true;
}

bool MatchAnalyzer::CanMatch(const EndpointInfo& writer, const EndpointInfo& reader) {
    // 1. Topic match
    if (writer.topic_name != reader.topic_name) {
        return false;
    }

    // 2. Type match
    if (!writer.type_name.empty() && !reader.type_name.empty()) {
        if (writer.type_name != reader.type_name) {
            return false;
        }
    }

    // 3. QoS compatibility (simplified check)
    // Note: For a full implementation, we'd need to parse QoS policies from
    // SEDP DATA messages. For now, we assume compatible if topic matches.
    // TODO: Implement QoS compatibility check:
    //       - Reliability: RELIABLE writer can match BEST_EFFORT reader
    //       - Durability: VOLATILE writer can match any reader
    //       - Ownership: EXCLUSIVE writer can match SHARED/EXCLUSIVE reader

    return true;
}

}  // namespace ondemand_monitor
