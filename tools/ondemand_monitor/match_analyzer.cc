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
    const uint8_t* ptr = params;
    const uint8_t* end = params + len;

    while (ptr + 4 <= end) {
        uint16_t current_pid = *reinterpret_cast<const uint16_t*>(ptr);
        uint16_t param_len = *reinterpret_cast<const uint16_t*>(ptr + 2);

        if (current_pid == Pid::PID_SENTINEL) {
            break;
        }

        if (current_pid == pid) {
            return ptr + 4;  // Return data pointer
        }

        ptr += 4 + param_len;
        // Align to 4 bytes
        uint16_t remainder = param_len % 4;
        if (remainder > 0) {
            ptr += 4 - remainder;
        }
    }

    return nullptr;
}

bool MatchAnalyzer::ExtractTopicName(const uint8_t* inline_qos, uint16_t len, std::string& out) {
    const uint8_t* param = FindParameter(inline_qos, len, Pid::PID_TOPIC_NAME);
    if (!param) {
        return false;
    }

    // TopicName is a string (uint32_t length + chars)
    uint32_t str_len = *reinterpret_cast<const uint32_t*>(param);
    if (str_len == 0 || str_len > static_cast<uint32_t>(len) - 4) {
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

    // TypeName is a string (uint32_t length + chars)
    uint32_t str_len = *reinterpret_cast<const uint32_t*>(param);
    if (str_len == 0 || str_len > static_cast<uint32_t>(len) - 4) {
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
