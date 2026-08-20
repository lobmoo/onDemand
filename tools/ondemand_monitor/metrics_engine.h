#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include "rtps_parser.h"

namespace ondemand_monitor {

// Participant (DomainParticipant)
struct ParticipantInfo {
    GUID_t guid;
    std::string name;
    uint32_t domain_id = 0;  // DDS Domain ID
    std::vector<Locator_t> locators;
    uint64_t first_seen_us;
    uint64_t last_seen_us;
    uint64_t last_pdp_seen_us = 0;  // Last PDP (discovery) packet timestamp
    uint32_t endpoints_count = 0;
    uint64_t heartbeat_count = 0;  // Heartbeat packets count
    uint64_t acknack_count = 0;    // ACKNACK packets count
    uint64_t nack_count = 0;       // NACK packets count
    bool is_active = false;  // true if PDP packet seen within last 5 seconds
};

// Endpoint (DataWriter or DataReader)
struct EndpointInfo {
    GUID_t guid;
    GUID_t participant_guid;
    bool is_writer = false;
    bool is_reader = false;
    std::string topic_name;
    std::string type_name;
    uint64_t first_seen_us;
    uint64_t last_seen_us;

    // Matched endpoints
    std::vector<GUID_t> matched_guids;

    // Transfer statistics
    uint64_t data_count = 0;
    uint64_t bytes_sent = 0;
    SequenceNumber_t last_sn = {0, 0};
    SequenceNumber_t expected_sn = {0, 0};  // Next expected sequence number
    uint64_t lost_count = 0;    // Number of detected lost packets
    uint64_t heartbeat_count = 0;
    uint64_t acknack_count = 0;
    uint64_t nack_count = 0;  // Number of NACKed sequences
};

// Endpoint pair transfer stats
struct TransferStats {
    GUID_t writer_guid;
    GUID_t reader_guid;
    uint64_t data_count = 0;
    uint64_t bytes_transferred = 0;
    uint64_t heartbeat_count = 0;
    uint64_t acknack_count = 0;
    uint64_t nack_count = 0;
    SequenceNumber_t last_writer_sn = {0, 0};
    SequenceNumber_t last_reader_ack_sn = {0, 0};
    uint64_t last_transfer_us = 0;
};

class MetricsEngine {
public:
    MetricsEngine() = default;

    // Feed parsed RTPS submessages
    void OnData(const DataSubmessage& data, uint64_t timestamp_us);
    void OnHeartbeat(const HeartbeatSubmessage& hb, uint64_t timestamp_us);
    void OnAcknack(const AcknackSubmessage& ack, uint64_t timestamp_us);

    // Query API (thread-safe)
    std::vector<ParticipantInfo> GetParticipants();
    std::vector<EndpointInfo> GetEndpoints(const GUID_t& participant_guid) const;
    std::vector<TransferStats> GetTransferStats() const;

    // Get unique topics for a participant
    struct TopicInfo {
        std::string topic_name;
        std::string type_name;
        bool has_writer = false;
        bool has_reader = false;
        uint64_t data_count = 0;
        uint64_t bytes_sent = 0;
        uint64_t nack_count = 0;
    };
    std::vector<TopicInfo> GetParticipantTopics(const GUID_t& participant_guid) const;

    // Global topic matching info
    struct TopicMatchInfo {
        std::string topic_name;
        std::string type_name;
        std::vector<std::string> writer_participants;  // participant names that have writers
        std::vector<std::string> reader_participants;  // participant names that have readers
        bool is_matched = false;  // true if has both writer and reader
    };
    std::vector<TopicMatchInfo> GetAllTopicMatches() const;

    // Summary stats
    struct Summary {
        uint32_t total_participants = 0;
        uint32_t total_endpoints = 0;
        uint64_t total_data_messages = 0;
        uint64_t total_bytes = 0;
        uint64_t total_heartbeats = 0;
        uint64_t total_acknacks = 0;
        uint64_t total_nacks = 0;
    };
    Summary GetSummary() const;

private:
    GUID_t GetParticipantGuid(const GUID_t& endpoint_guid) const;
    void UpdateEndpoint(const GUID_t& guid, bool is_writer, uint64_t timestamp_us);
    void UpdateTransferPair(const GUID_t& writer, const GUID_t& reader, uint64_t timestamp_us);

    mutable std::shared_mutex mutex_;

    std::unordered_map<GUID_t, ParticipantInfo, GUIDHash> participants_;
    std::unordered_map<GUID_t, EndpointInfo, GUIDHash> endpoints_;

    // Hash for GUID prefix (first 12 bytes)
    struct GUIDPrefixHash {
        size_t operator()(const std::array<uint8_t, 12>& prefix) const {
            size_t h = 0;
            for (auto b : prefix) h ^= b + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    // Mapping from endpoint GUID prefix to participant GUID (populated from SPDP)
    // Key: endpoint GUID prefix (first 12 bytes), Value: participant GUID
    std::unordered_map<std::array<uint8_t, 12>, GUID_t, GUIDPrefixHash> prefix_to_participant_;

    // Endpoint GUID → participant GUID mapping (from SEDP endpoint_guid + participant_name)
    // This allows distinguishing endpoints when multiple participants share the same GUID prefix
    std::unordered_map<GUID_t, GUID_t, GUIDHash> endpoint_to_participant_;
    // Participant name → participant GUID mapping
    std::unordered_map<std::string, GUID_t> name_to_participant_;
    // Pending SEDP endpoints: endpoint GUID → participant name (stored before SPDP arrives)
    std::unordered_map<GUID_t, std::string, GUIDHash> sedp_endpoint_name_;
    // Cached pub/sub participant GUIDs (support multiple subscribers)
    // Key: participant GUID, Value: true if pub, false if sub
    std::unordered_map<GUID_t, bool, GUIDHash> discovered_pub_sub_;
    // Quick lookup: GUID prefix → participant GUID for pub/sub nodes
    std::unordered_map<std::array<uint8_t, 12>, GUID_t, GUIDPrefixHash> prefix_to_pub_sub_;

    // Transfer stats: key = hash(writer_guid, reader_guid)
    struct PairKey {
        GUID_t writer;
        GUID_t reader;
        bool operator==(const PairKey& o) const {
            return writer == o.writer && reader == o.reader;
        }
    };
    struct PairKeyHash {
        size_t operator()(const PairKey& k) const {
            return GUIDHash{}(k.writer) ^ (GUIDHash{}(k.reader) << 1);
        }
    };
    std::unordered_map<PairKey, TransferStats, PairKeyHash> transfers_;

    // Global counters
    uint64_t total_data_messages_ = 0;
    uint64_t total_bytes_ = 0;
    uint64_t total_heartbeats_ = 0;
    uint64_t total_acknacks_ = 0;
    uint64_t total_nacks_ = 0;
};

}  // namespace ondemand_monitor
