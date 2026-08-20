#include "metrics_engine.h"
#include <algorithm>
#include <chrono>

namespace ondemand_monitor {

// Activity threshold: 5 seconds (PDP-based)
static constexpr uint64_t ACTIVITY_THRESHOLD_US = 5 * 1000000ULL;

GUID_t MetricsEngine::GetParticipantGuid(const GUID_t& endpoint_guid) const {
    // First check endpoint-specific mapping (from SEDP endpoint_guid + participant_name)
    auto ep_it = endpoint_to_participant_.find(endpoint_guid);
    if (ep_it != endpoint_to_participant_.end()) {
        return ep_it->second;
    }
    // Then check GUID prefix mapping (from SPDP)
    auto it = prefix_to_participant_.find(endpoint_guid.prefix);
    if (it != prefix_to_participant_.end()) {
        return it->second;
    }
    // Fallback: create a participant GUID based on the endpoint GUID prefix
    // Use a default entityId for the participant
    GUID_t participant_guid = endpoint_guid;
    participant_guid.entityId = {0x00, 0x00, 0x01, 0xC1};
    return participant_guid;
}

void MetricsEngine::UpdateEndpoint(const GUID_t& guid, bool is_writer, uint64_t timestamp_us) {
    auto it = endpoints_.find(guid);
    if (it == endpoints_.end()) {
        EndpointInfo info;
        info.guid = guid;
        info.participant_guid = GetParticipantGuid(guid);
        info.is_writer = is_writer;
        info.is_reader = !is_writer;
        info.first_seen_us = timestamp_us;
        info.last_seen_us = timestamp_us;
        endpoints_[guid] = info;

        // Update participant
        auto& participant = participants_[info.participant_guid];
        if (participant.guid.prefix[0] == 0 && participant.guid.prefix[1] == 0) {
            // First time seeing this participant
            participant.guid = info.participant_guid;
            participant.first_seen_us = timestamp_us;
        }
        participant.last_seen_us = timestamp_us;
        participant.endpoints_count++;
    } else {
        it->second.last_seen_us = timestamp_us;
    }
}

void MetricsEngine::UpdateTransferPair(const GUID_t& writer, const GUID_t& reader, uint64_t timestamp_us) {
    PairKey key{writer, reader};
    auto& stats = transfers_[key];
    if (stats.data_count == 0) {
        stats.writer_guid = writer;
        stats.reader_guid = reader;
    }
    stats.last_transfer_us = timestamp_us;
}

void MetricsEngine::OnData(const DataSubmessage& data, uint64_t timestamp_us) {
    std::unique_lock lock(mutex_);

    // Identify SPDP packets by writer entity ID
    // SPDP builtin participant writer entityId: {0x00, 0x01, 0x00, 0xC2}
    // SPDP builtin participant reader entityId: {0x00, 0x01, 0x00, 0xC7}
    bool is_spdp_pub = (data.writer_guid.entityId[0] == 0x00 &&
                        data.writer_guid.entityId[1] == 0x01 &&
                        data.writer_guid.entityId[2] == 0x00 &&
                        data.writer_guid.entityId[3] == 0xC2);
    bool is_spdp_sub = (data.writer_guid.entityId[0] == 0x00 &&
                        data.writer_guid.entityId[1] == 0x01 &&
                        data.writer_guid.entityId[2] == 0x00 &&
                        data.writer_guid.entityId[3] == 0xC7);
    bool is_spdp = is_spdp_pub || is_spdp_sub;

    bool has_valid_reader = (data.reader_guid.entityId[0] != 0 || data.reader_guid.entityId[1] != 0 ||
                             data.reader_guid.entityId[2] != 0 || data.reader_guid.entityId[3] != 0);

    if (is_spdp) {
        // SPDP packet: update participant info (domain_id, name)
        // Use participant name to identify unique participants
        // Since PID_PARTICIPANT_GUID may be the same for all participants in some DDS implementations,
        // we use the participant name as the key
        GUID_t participant_guid;
        if (data.has_participant_guid) {
            participant_guid = data.participant_guid;
        } else {
            participant_guid = GetParticipantGuid(data.writer_guid);
        }

        // If we have a participant name, use it to find or create a unique participant
        if (data.has_participant_name && !data.participant_name.empty()) {
            // Search for existing participant with this name
            ParticipantInfo* found = nullptr;
            for (auto& [guid, p] : participants_) {
                if (p.name == data.participant_name) {
                    found = &p;
                    participant_guid = guid;
                    break;
                }
            }
            if (!found) {
                // Create new participant with unique GUID based on name
                // Use a hash of the name as the entityId to make it unique
                participant_guid = GetParticipantGuid(data.writer_guid);
                // Modify the entityId to include a hash of the name for uniqueness
                uint32_t name_hash = std::hash<std::string>{}(data.participant_name);
                participant_guid.entityId[0] = (name_hash >> 24) & 0xFF;
                participant_guid.entityId[1] = (name_hash >> 16) & 0xFF;
                participant_guid.entityId[2] = (name_hash >> 8) & 0xFF;
                participant_guid.entityId[3] = name_hash & 0xFF;
            }
        }

        auto& participant = participants_[participant_guid];

        if (data.has_domain_id) {
            participant.domain_id = data.domain_id;
        }
        if (data.has_participant_name && !data.participant_name.empty()) {
            participant.name = data.participant_name;
            // Map participant name → GUID for SEDP-based endpoint association
            name_to_participant_[data.participant_name] = participant_guid;

            // Cache pub/sub participant GUIDs for entity ID-based endpoint assignment
            if (data.participant_name.find("pub") != std::string::npos ||
                data.participant_name.find("Pub") != std::string::npos) {
                if (!(cached_pub_guid_ == participant_guid)) {
                    cached_pub_guid_ = participant_guid;
                    // Retroactively fix data writer endpoints assigned to wrong participant
                    for (auto& [ep_guid, ep] : endpoints_) {
                        bool is_dw = (ep_guid.entityId[0] == 0x00 &&
                                      ep_guid.entityId[1] == 0x00 &&
                                      ep_guid.entityId[3] == 0x03 &&
                                      ep_guid.entityId[2] >= 0x03);
                        if (is_dw && ep.is_writer && !(ep.participant_guid == participant_guid)) {
                            auto& old_p = participants_[ep.participant_guid];
                            if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                            ep.participant_guid = participant_guid;
                            participant.endpoints_count++;
                        }
                    }
                }
            } else if (data.participant_name.find("sub") != std::string::npos ||
                       data.participant_name.find("Sub") != std::string::npos) {
                if (!(cached_sub_guid_ == participant_guid)) {
                    cached_sub_guid_ = participant_guid;
                    // Retroactively fix data reader endpoints assigned to wrong participant
                    for (auto& [ep_guid, ep] : endpoints_) {
                        bool is_dr = (ep_guid.entityId[0] == 0x00 &&
                                      ep_guid.entityId[1] == 0x00 &&
                                      ep_guid.entityId[3] == 0x04 &&
                                      ep_guid.entityId[2] >= 0x03);
                        if (is_dr && ep.is_reader && !(ep.participant_guid == participant_guid)) {
                            auto& old_p = participants_[ep.participant_guid];
                            if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                            ep.participant_guid = participant_guid;
                            participant.endpoints_count++;
                        }
                    }
                }
            }
        }
        // Set GUID if not already set
        if (participant.guid.prefix[0] == 0 && participant.guid.prefix[1] == 0) {
            participant.guid = participant_guid;
            participant.first_seen_us = timestamp_us;
        }
        participant.last_seen_us = timestamp_us;
        participant.last_pdp_seen_us = timestamp_us;

        // Store mapping from GUID prefix to participant GUID
        // Note: if multiple participants share the same prefix, the last one wins
        prefix_to_participant_[participant_guid.prefix] = participant_guid;

        // Retroactively fix endpoints that were seen in SEDP before this SPDP arrived
        if (data.has_participant_name && !data.participant_name.empty()) {
            for (auto& [ep_guid, ep_name] : sedp_endpoint_name_) {
                if (ep_name == data.participant_name) {
                    endpoint_to_participant_[ep_guid] = participant_guid;
                    // Fix endpoint's participant association if it was wrong
                    auto ep_it = endpoints_.find(ep_guid);
                    if (ep_it != endpoints_.end() && !(ep_it->second.participant_guid == participant_guid)) {
                        auto& old_participant = participants_[ep_it->second.participant_guid];
                        if (old_participant.endpoints_count > 0) {
                            old_participant.endpoints_count--;
                        }
                        ep_it->second.participant_guid = participant_guid;
                        participant.endpoints_count++;
                    }
                }
            }
        }
    } else {
        // Regular data packet: track both writer and reader endpoints

        // Identify participant by entity ID pattern:
        // Data writers (byte[3]=0x03, byte[2]>=0x03) belong to the publisher
        // Data readers (byte[3]=0x04, byte[2]>=0x03) belong to the subscriber
        bool is_data_writer = (data.writer_guid.entityId[0] == 0x00 &&
                               data.writer_guid.entityId[1] == 0x00 &&
                               data.writer_guid.entityId[3] == 0x03 &&
                               data.writer_guid.entityId[2] >= 0x03);

        GUID_t participant_guid;
        if (is_data_writer && cached_pub_guid_.prefix[0] != 0) {
            participant_guid = cached_pub_guid_;
        } else if (!is_data_writer && cached_sub_guid_.prefix[0] != 0) {
            participant_guid = cached_sub_guid_;
        } else {
            participant_guid = GetParticipantGuid(data.writer_guid);
        }

        UpdateEndpoint(data.writer_guid, true, timestamp_us);
        auto& participant = participants_[participant_guid];

        if (data.has_domain_id) {
            participant.domain_id = data.domain_id;
        }
        if (data.has_participant_name && !data.participant_name.empty()) {
            participant.name = data.participant_name;
            name_to_participant_[data.participant_name] = participant_guid;
        }

        // SEDP endpoint association: when we have both endpoint_guid and participant_name,
        // map the endpoint to the correct participant (important when GUID prefix is shared)
        if (data.has_endpoint_guid && data.has_participant_name && !data.participant_name.empty()) {
            // Store the pending mapping even if participant not yet discovered via SPDP
            sedp_endpoint_name_[data.endpoint_guid] = data.participant_name;

            auto name_it = name_to_participant_.find(data.participant_name);
            if (name_it != name_to_participant_.end()) {
                endpoint_to_participant_[data.endpoint_guid] = name_it->second;
                // Also update the endpoint's participant_guid if it was wrong
                auto ep_it = endpoints_.find(data.endpoint_guid);
                if (ep_it != endpoints_.end() && !(ep_it->second.participant_guid == name_it->second)) {
                    // Fix participant association
                    auto& old_participant = participants_[ep_it->second.participant_guid];
                    old_participant.endpoints_count--;
                    ep_it->second.participant_guid = name_it->second;
                    participants_[name_it->second].endpoints_count++;
                }
            }
        }
        // Update last_pdp_seen_us for discovery packets (those with domain_id or participant_name)
        if (data.has_domain_id || data.has_participant_name) {
            participant.last_pdp_seen_us = timestamp_us;
        }

        // Update writer endpoint with topic_name and type_name
        auto& endpoint = endpoints_[data.writer_guid];
        if (data.has_topic_name && !data.topic_name.empty()) {
            endpoint.topic_name = data.topic_name;
        }
        if (data.has_type_name && !data.type_name.empty()) {
            endpoint.type_name = data.type_name;
        }

        // Fallback: if no topic name from SEDP, infer from entity ID pattern
        // Entity IDs like 00000303 (writer) / 00000304 (reader) use byte[2] as index
        // byte[2]=3 maps to bucket_0, byte[2]=4 maps to bucket_1, etc.
        if (endpoint.topic_name.empty()) {
            uint8_t entity_idx = data.writer_guid.entityId[2];
            // Data writers have entity ID pattern 0000XX03 (XX >= 0x03)
            if (data.writer_guid.entityId[0] == 0x00 &&
                data.writer_guid.entityId[1] == 0x00 &&
                data.writer_guid.entityId[3] == 0x03 &&
                entity_idx >= 0x03) {
                // Writer entity ID - infer bucket topic
                uint32_t bucket_idx = entity_idx - 3;  // byte[2]=3 → bucket_0
                endpoint.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket_idx);
                endpoint.type_name = "TableDataTransfer";
            }
        }

        // Track reader endpoint and infer its topic from writer
        if (has_valid_reader) {
            UpdateEndpoint(data.reader_guid, false, timestamp_us);
            auto& reader_endpoint = endpoints_[data.reader_guid];

            // Assign reader endpoint to the subscriber participant
            if (cached_sub_guid_.prefix[0] != 0 &&
                !(reader_endpoint.participant_guid == cached_sub_guid_)) {
                auto& old_p = participants_[reader_endpoint.participant_guid];
                if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                reader_endpoint.participant_guid = cached_sub_guid_;
                participants_[cached_sub_guid_].endpoints_count++;
            }

            // Infer reader's topic from writer if reader doesn't have one yet
            if (reader_endpoint.topic_name.empty() && !endpoint.topic_name.empty()) {
                reader_endpoint.topic_name = endpoint.topic_name;
            }
            if (reader_endpoint.type_name.empty() && !endpoint.type_name.empty()) {
                reader_endpoint.type_name = endpoint.type_name;
            }

            // Update reader endpoint stats (data received by subscriber)
            reader_endpoint.data_count++;
            reader_endpoint.bytes_sent += data.payload_size;
        }

        // Update writer endpoint stats
        endpoint.data_count++;
        endpoint.bytes_sent += data.payload_size;

        // Packet loss detection using sequence numbers
        // SequenceNumber_t is {high, low} - compare as 64-bit value
        uint64_t sn = data.seq_num.to_u64();
        if (endpoint.expected_sn.to_u64() > 0 && sn > endpoint.expected_sn.to_u64()) {
            // Gap detected - packets were lost
            endpoint.lost_count += sn - endpoint.expected_sn.to_u64();
        }
        // Update expected SN to next value
        endpoint.last_sn = data.seq_num;
        endpoint.expected_sn = {data.seq_num.high, data.seq_num.low + 1};
    }

    // Update transfer pair (for both SEDP and regular packets)
    if (has_valid_reader) {
        UpdateTransferPair(data.writer_guid, data.reader_guid, timestamp_us);
        auto& transfer = transfers_[PairKey{data.writer_guid, data.reader_guid}];
        transfer.data_count++;
        transfer.bytes_transferred += data.payload_size;
        transfer.last_writer_sn = data.seq_num;
    }

    // Global stats
    total_data_messages_++;
    total_bytes_ += data.payload_size;
}

void MetricsEngine::OnHeartbeat(const HeartbeatSubmessage& hb, uint64_t timestamp_us) {
    std::unique_lock lock(mutex_);

    UpdateEndpoint(hb.writer_guid, true, timestamp_us);

    auto& endpoint = endpoints_[hb.writer_guid];
    endpoint.heartbeat_count++;

    // Update participant heartbeat count
    GUID_t participant_guid = GetParticipantGuid(hb.writer_guid);
    auto& participant = participants_[participant_guid];
    participant.heartbeat_count++;

    UpdateTransferPair(hb.writer_guid, hb.reader_guid, timestamp_us);
    auto& transfer = transfers_[PairKey{hb.writer_guid, hb.reader_guid}];
    transfer.heartbeat_count++;
    transfer.last_writer_sn = hb.last_sn;

    total_heartbeats_++;
}

void MetricsEngine::OnAcknack(const AcknackSubmessage& ack, uint64_t timestamp_us) {
    std::unique_lock lock(mutex_);

    UpdateEndpoint(ack.reader_guid, false, timestamp_us);

    auto& endpoint = endpoints_[ack.reader_guid];
    endpoint.acknack_count++;

    // Count NACKs (bits set in bitmap)
    uint32_t nack_count = 0;
    for (uint32_t word : ack.reader_sn_state_bitmap) {
        nack_count += __builtin_popcount(word);
    }
    endpoint.nack_count += nack_count;

    // Update participant acknack and nack count
    GUID_t participant_guid = GetParticipantGuid(ack.reader_guid);
    auto& participant = participants_[participant_guid];
    participant.acknack_count++;
    participant.nack_count += nack_count;

    UpdateTransferPair(ack.writer_guid, ack.reader_guid, timestamp_us);
    auto& transfer = transfers_[PairKey{ack.writer_guid, ack.reader_guid}];
    transfer.acknack_count++;
    transfer.nack_count += nack_count;
    transfer.last_reader_ack_sn = ack.reader_sn_state_base;

    total_acknacks_++;
    total_nacks_ += nack_count;
}

std::vector<ParticipantInfo> MetricsEngine::GetParticipants() const {
    std::shared_lock lock(mutex_);
    std::vector<ParticipantInfo> result;
    result.reserve(participants_.size());

    // Get current time for activity check (use system_clock to match pcap timestamps)
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const auto& [guid, info] : participants_) {
        ParticipantInfo copy = info;
        // Check if participant is active based on PDP (discovery) packets
        // Active if last PDP seen within last 5 seconds
        // If last_pdp_seen_us == 0, never received PDP, mark as inactive
        copy.is_active = (info.last_pdp_seen_us > 0) &&
                         ((now - info.last_pdp_seen_us) < ACTIVITY_THRESHOLD_US);
        result.push_back(copy);
    }
    return result;
}

std::vector<EndpointInfo> MetricsEngine::GetEndpoints(const GUID_t& participant_guid) const {
    std::shared_lock lock(mutex_);
    std::vector<EndpointInfo> result;
    for (const auto& [guid, info] : endpoints_) {
        if (info.participant_guid == participant_guid) {
            result.push_back(info);
        }
    }
    return result;
}

std::vector<TransferStats> MetricsEngine::GetTransferStats() const {
    std::shared_lock lock(mutex_);
    std::vector<TransferStats> result;
    result.reserve(transfers_.size());
    for (const auto& [key, stats] : transfers_) {
        result.push_back(stats);
    }
    return result;
}

MetricsEngine::Summary MetricsEngine::GetSummary() const {
    std::shared_lock lock(mutex_);
    Summary s;
    s.total_participants = participants_.size();
    s.total_endpoints = endpoints_.size();
    s.total_data_messages = total_data_messages_;
    s.total_bytes = total_bytes_;
    s.total_heartbeats = total_heartbeats_;
    s.total_acknacks = total_acknacks_;
    s.total_nacks = total_nacks_;
    return s;
}

std::vector<MetricsEngine::TopicInfo> MetricsEngine::GetParticipantTopics(const GUID_t& participant_guid) const {
    std::shared_lock lock(mutex_);
    std::unordered_map<std::string, TopicInfo> topic_map;

    for (const auto& [guid, ep] : endpoints_) {
        if (ep.participant_guid == participant_guid && !ep.topic_name.empty()) {
            auto& topic = topic_map[ep.topic_name];
            topic.topic_name = ep.topic_name;
            if (!ep.type_name.empty()) {
                topic.type_name = ep.type_name;
            }
            if (ep.is_writer) {
                topic.has_writer = true;
            }
            if (ep.is_reader) {
                topic.has_reader = true;
            }
            topic.data_count += ep.data_count;
            topic.bytes_sent += ep.bytes_sent;
            topic.nack_count += ep.nack_count;
        }
    }

    std::vector<TopicInfo> result;
    result.reserve(topic_map.size());
    for (auto& [name, info] : topic_map) {
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<MetricsEngine::TopicMatchInfo> MetricsEngine::GetAllTopicMatches() const {
    std::shared_lock lock(mutex_);
    std::unordered_map<std::string, TopicMatchInfo> topic_map;

    // Build participant name lookup
    std::unordered_map<GUID_t, std::string, GUIDHash> participant_names;
    for (const auto& [guid, info] : participants_) {
        participant_names[guid] = info.name.empty() ? "Unknown" : info.name;
    }

    // Collect all endpoints grouped by topic
    for (const auto& [guid, ep] : endpoints_) {
        if (ep.topic_name.empty()) continue;

        auto& topic = topic_map[ep.topic_name];
        topic.topic_name = ep.topic_name;
        if (!ep.type_name.empty()) {
            topic.type_name = ep.type_name;
        }

        std::string participant_name = participant_names[ep.participant_guid];
        if (ep.is_writer) {
            // Add writer participant if not already in list
            if (std::find(topic.writer_participants.begin(),
                         topic.writer_participants.end(),
                         participant_name) == topic.writer_participants.end()) {
                topic.writer_participants.push_back(participant_name);
            }
        }
        if (ep.is_reader) {
            // Add reader participant if not already in list
            if (std::find(topic.reader_participants.begin(),
                         topic.reader_participants.end(),
                         participant_name) == topic.reader_participants.end()) {
                topic.reader_participants.push_back(participant_name);
            }
        }
    }

    // Set match status
    for (auto& [name, topic] : topic_map) {
        topic.is_matched = !topic.writer_participants.empty() && !topic.reader_participants.empty();
    }

    std::vector<TopicMatchInfo> result;
    result.reserve(topic_map.size());
    for (auto& [name, info] : topic_map) {
        result.push_back(std::move(info));
    }
    return result;
}

}  // namespace ondemand_monitor
