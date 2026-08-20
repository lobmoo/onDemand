#pragma once

#include "metrics_engine.h"

namespace ondemand_monitor {

// Analyze topic and QoS matching between endpoints
class MatchAnalyzer {
public:
    // Check if a writer and reader can communicate
    // (topic match, type match, QoS compatibility)
    static bool CanMatch(const EndpointInfo& writer, const EndpointInfo& reader);

    // Extract topic name from DATA inline QoS (PID_TOPIC_NAME = 0x0005)
    static bool ExtractTopicName(const uint8_t* inline_qos, uint16_t len, std::string& out);

    // Extract type name from DATA inline QoS (PID_TYPE_NAME = 0x0007)
    static bool ExtractTypeName(const uint8_t* inline_qos, uint16_t len, std::string& out);

private:
    // Parse ParameterList (used in InlineQoS)
    static const uint8_t* FindParameter(const uint8_t* params, uint16_t len, uint16_t pid);
};

}  // namespace ondemand_monitor
