#include "metrics_engine.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace ondemand_monitor {

// Activity threshold: 5 seconds (PDP-based)
static constexpr uint64_t ACTIVITY_THRESHOLD_US = 5 * 1000000ULL;
// Cleanup threshold: 10 seconds - remove inactive participants
static constexpr uint64_t CLEANUP_THRESHOLD_US = 10 * 1000000ULL;

GUID_t MetricsEngine::GetParticipantGuid(const GUID_t& endpoint_guid) const {
    // First check endpoint-specific mapping (from SEDP endpoint_guid + participant_name)
    auto ep_it = endpoint_to_participant_.find(endpoint_guid);
    if (ep_it != endpoint_to_participant_.end()) {
        return ep_it->second;
    }

    // Check pub/sub specific mapping (more accurate for multi-node scenarios)
    auto pubsub_it = prefix_to_pub_sub_.find(endpoint_guid.prefix);
    if (pubsub_it != prefix_to_pub_sub_.end()) {
        return pubsub_it->second;
    }

    // Then check general GUID prefix mapping (from SPDP)
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
        GUID_t participant_guid;
        if (data.has_participant_guid) {
            participant_guid = data.participant_guid;
        } else {
            participant_guid = GetParticipantGuid(data.writer_guid);
        }

        // Skip Domain 0 - it's FastDDS internal discovery, not user data
        if (data.has_domain_id && data.domain_id == 0) {
            return;
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
                participant_guid = GetParticipantGuid(data.writer_guid);
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
            bool is_pub = (data.participant_name.find("pub") != std::string::npos ||
                           data.participant_name.find("Pub") != std::string::npos);
            bool is_sub = (data.participant_name.find("sub") != std::string::npos ||
                           data.participant_name.find("Sub") != std::string::npos);

            if (is_pub || is_sub) {
                // Register this participant as pub/sub
                bool is_new = (discovered_pub_sub_.find(participant_guid) == discovered_pub_sub_.end());
                discovered_pub_sub_[participant_guid] = is_pub;
                prefix_to_pub_sub_[participant_guid.prefix] = participant_guid;

                if (is_new && is_sub) {
                    // When a new subscriber is discovered, create20 reader endpoints
                    // (one per bucket) with the subscriber's GUID prefix.
                    // Entity ID pattern: {00, 00, bucket_idx+3, 04}
                    for (uint32_t bucket = 0; bucket < 20; bucket++) {
                        GUID_t reader_guid;
                        std::memcpy(reader_guid.prefix.data(), participant_guid.prefix.data(), 12);
                        reader_guid.entityId[0] = 0x00;
                        reader_guid.entityId[1] = 0x00;
                        reader_guid.entityId[2] = static_cast<uint8_t>(bucket + 3);
                        reader_guid.entityId[3] = 0x04;  // Reader

                        UpdateEndpoint(reader_guid, false, timestamp_us);
                        auto& ep = endpoints_[reader_guid];
                        ep.is_reader = true;
                        ep.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket);
                        ep.type_name = "TableDataTransfer";
                        ep.participant_guid = participant_guid;
                        endpoint_to_participant_[reader_guid] = participant_guid;
                    }

                    // Also fix any existing writer endpoints to pub participant
                    GUID_t pub_guid;
                    for (const auto& [guid, is_pub_flag] : discovered_pub_sub_) {
                        if (is_pub_flag) { pub_guid = guid; break; }
                    }
                    if (pub_guid.prefix[0] != 0) {
                        for (auto& [ep_guid, ep] : endpoints_) {
                            if (ep.is_writer && ep_guid.entityId[3] == 0x03 &&
                                ep_guid.entityId[2] >= 0x03) {
                                if (!(ep.participant_guid == pub_guid)) {
                                    auto& old_p = participants_[ep.participant_guid];
                                    if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                                    ep.participant_guid = pub_guid;
                                    participants_[pub_guid].endpoints_count++;
                                }
                            }
                        }
                    }
                } else if (is_new && is_pub) {
                    // Fix writer endpoints: assign to pub participant
                    for (auto& [ep_guid, ep] : endpoints_) {
                        if (ep.is_writer && ep_guid.entityId[3] == 0x03 &&
                            ep_guid.entityId[2] >= 0x03) {
                            if (!(ep.participant_guid == participant_guid)) {
                                auto& old_p = participants_[ep.participant_guid];
                                if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                                ep.participant_guid = participant_guid;
                                participants_[participant_guid].endpoints_count++;
                            }
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
        // Non-SPDP packet: could be SEDP discovery or regular data

        // SEDP endpoint handling: SEDP packets contain the REAL endpoint GUID
        // (with subscriber's GUID prefix). Use SEDP to create reader endpoints.
        // Detection: SEDP packets have endpoint_guid (PID_ENDPOINT_GUID), data packets don't.
        bool is_sedp = data.has_endpoint_guid;

        if (is_sedp) {
            // This is a SEDP discovery packet - handle endpoint creation
            if (data.has_participant_name && !data.participant_name.empty()) {
                sedp_endpoint_name_[data.endpoint_guid] = data.participant_name;
            }

            // Create/update endpoint from SEDP (this has the correct GUID prefix)
            UpdateEndpoint(data.endpoint_guid, data.has_topic_name, timestamp_us);
            auto& sedp_endpoint = endpoints_[data.endpoint_guid];

            // Set topic/type from SEDP payload
            if (data.has_topic_name && !data.topic_name.empty()) {
                sedp_endpoint.topic_name = data.topic_name;
            }
            if (data.has_type_name && !data.type_name.empty()) {
                sedp_endpoint.type_name = data.type_name;
            }
            // Infer topic/type from entity ID if not set
            if (sedp_endpoint.topic_name.empty()) {
                uint8_t entity_idx = data.endpoint_guid.entityId[2];
                if (data.endpoint_guid.entityId[0] == 0x00 &&
                    data.endpoint_guid.entityId[1] == 0x00 &&
                    entity_idx >= 0x03) {
                    uint32_t bucket_idx = entity_idx - 3;
                    if (data.endpoint_guid.entityId[3] == 0x04) {
                        sedp_endpoint.is_reader = true;
                        sedp_endpoint.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket_idx);
                        sedp_endpoint.type_name = "TableDataTransfer";
                    } else if (data.endpoint_guid.entityId[3] == 0x03) {
                        sedp_endpoint.is_writer = true;
                        sedp_endpoint.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket_idx);
                        sedp_endpoint.type_name = "TableDataTransfer";
                    }
                }
            }

            // Assign SEDP endpoint to participant
            // Try by name lookup first, then by GUID prefix
            bool assigned = false;
            if (data.has_participant_name && !data.participant_name.empty()) {
                auto name_it = name_to_participant_.find(data.participant_name);
                if (name_it != name_to_participant_.end()) {
                    endpoint_to_participant_[data.endpoint_guid] = name_it->second;
                    if (!(sedp_endpoint.participant_guid == name_it->second)) {
                        auto& old_p = participants_[sedp_endpoint.participant_guid];
                        if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                        sedp_endpoint.participant_guid = name_it->second;
                        participants_[name_it->second].endpoints_count++;
                    }
                    assigned = true;
                    // Update discovery timestamp
                    participants_[name_it->second].last_pdp_seen_us = timestamp_us;
                }
            }
            // Fallback: find participant by GUID prefix
            if (!assigned) {
                auto prefix_it = prefix_to_participant_.find(data.endpoint_guid.prefix);
                if (prefix_it != prefix_to_participant_.end()) {
                    endpoint_to_participant_[data.endpoint_guid] = prefix_it->second;
                    if (!(sedp_endpoint.participant_guid == prefix_it->second)) {
                        auto& old_p = participants_[sedp_endpoint.participant_guid];
                        if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                        sedp_endpoint.participant_guid = prefix_it->second;
                        participants_[prefix_it->second].endpoints_count++;
                    }
                    assigned = true;
                }
            }
            // If still not assigned, store for retroactive assignment when SPDP arrives
            if (!assigned && data.has_participant_name && !data.participant_name.empty()) {
                // Will be assigned later in SPDP handler
            }
            return;  // SEDP handled, skip data processing
        }

        // Regular data packet: track writer endpoints and reader stats

        // Find participant for writer using entity ID pattern
        bool is_data_writer = (data.writer_guid.entityId[0] == 0x00 &&
                               data.writer_guid.entityId[1] == 0x00 &&
                               data.writer_guid.entityId[3] == 0x03 &&
                               data.writer_guid.entityId[2] >= 0x03);

        GUID_t writer_participant_guid;
        if (is_data_writer) {
            for (const auto& [guid, is_pub] : discovered_pub_sub_) {
                if (is_pub) {
                    writer_participant_guid = guid;
                    break;
                }
            }
        }
        if (writer_participant_guid.prefix[0] == 0) {
            writer_participant_guid = GetParticipantGuid(data.writer_guid);
        }

        UpdateEndpoint(data.writer_guid, true, timestamp_us);
        auto& participant = participants_[writer_participant_guid];

        if (data.has_domain_id) {
            participant.domain_id = data.domain_id;
        }
        if (data.has_participant_name && !data.participant_name.empty()) {
            participant.name = data.participant_name;
            name_to_participant_[data.participant_name] = writer_participant_guid;
        }

        // SEDP endpoint handling: SEDP packets contain the REAL endpoint GUID
        // (with subscriber's GUID prefix). Use SEDP to create reader endpoints.
        if (data.has_endpoint_guid && data.has_participant_name && !data.participant_name.empty()) {
            sedp_endpoint_name_[data.endpoint_guid] = data.participant_name;

            // Create/update endpoint from SEDP (this has the correct GUID prefix)
            UpdateEndpoint(data.endpoint_guid, data.has_topic_name, timestamp_us);
            auto& sedp_endpoint = endpoints_[data.endpoint_guid];

            // Set topic/type from SEDP payload
            if (data.has_topic_name && !data.topic_name.empty()) {
                sedp_endpoint.topic_name = data.topic_name;
            }
            if (data.has_type_name && !data.type_name.empty()) {
                sedp_endpoint.type_name = data.type_name;
            }
            // Infer topic from entity ID if not set
            if (sedp_endpoint.topic_name.empty()) {
                uint8_t entity_idx = data.endpoint_guid.entityId[2];
                if (data.endpoint_guid.entityId[0] == 0x00 &&
                    data.endpoint_guid.entityId[1] == 0x00 &&
                    entity_idx >= 0x03) {
                    uint32_t bucket_idx = entity_idx - 3;
                    // Check if it's a reader (0x04) or writer (0x03)
                    if (data.endpoint_guid.entityId[3] == 0x04) {
                        sedp_endpoint.is_reader = true;
                        sedp_endpoint.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket_idx);
                        sedp_endpoint.type_name = "TableDataTransfer";
                    } else if (data.endpoint_guid.entityId[3] == 0x03) {
                        sedp_endpoint.is_writer = true;
                        sedp_endpoint.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket_idx);
                        sedp_endpoint.type_name = "TableDataTransfer";
                    }
                }
            }

            // Assign endpoint to participant
            auto name_it = name_to_participant_.find(data.participant_name);
            if (name_it != name_to_participant_.end()) {
                endpoint_to_participant_[data.endpoint_guid] = name_it->second;
                if (!(sedp_endpoint.participant_guid == name_it->second)) {
                    auto& old_p = participants_[sedp_endpoint.participant_guid];
                    if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                    sedp_endpoint.participant_guid = name_it->second;
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

        // Track reader stats: DATA packets use pub's prefix for reader GUID,
        // so we match by entity ID to find the real reader endpoint (from SEDP).
        if (has_valid_reader) {
            // Find existing reader endpoints with matching entity ID
            // (SEDP creates endpoints with sub's prefix, DATA has pub's prefix)
            for (auto& [ep_guid, ep] : endpoints_) {
                if (ep.is_reader &&
                    ep_guid.entityId[0] == data.reader_guid.entityId[0] &&
                    ep_guid.entityId[1] == data.reader_guid.entityId[1] &&
                    ep_guid.entityId[2] == data.reader_guid.entityId[2] &&
                    ep_guid.entityId[3] == data.reader_guid.entityId[3]) {
                    ep.data_count++;
                    ep.bytes_sent += data.payload_size;
                    ep.last_seen_us = timestamp_us;  // Update last seen for frequency calc
                }
            }
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

std::vector<ParticipantInfo> MetricsEngine::GetParticipants() {
    std::unique_lock lock(mutex_);
    std::vector<ParticipantInfo> result;

    // Get current time for activity check
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Cleanup: remove participants inactive for more than 10 seconds
    auto it = participants_.begin();
    while (it != participants_.end()) {
        auto& [guid, info] = *it;
        bool should_remove = false;

        // Remove if inactive for too long (10 seconds since last PDP/SPDP)
        if (info.last_pdp_seen_us > 0 && (now - info.last_pdp_seen_us) > CLEANUP_THRESHOLD_US) {
            should_remove = true;
        }
        // Also remove if never got PDP/SPDP and inactive for 10 seconds since last seen
        if (info.last_pdp_seen_us == 0 && info.last_seen_us > 0 &&
            (now - info.last_seen_us) > CLEANUP_THRESHOLD_US) {
            should_remove = true;
        }
        // NOTE: We do NOT remove domain_id==0 participants here because
        // data packets may arrive before SPDP discovery (which sets domain_id).
        // Domain 0 filtering is done in the UI display layer instead.

        if (should_remove) {
            // Remove associated endpoints
            auto ep_it = endpoints_.begin();
            while (ep_it != endpoints_.end()) {
                if (ep_it->second.participant_guid == guid) {
                    ep_it = endpoints_.erase(ep_it);
                } else {
                    ++ep_it;
                }
            }
            // Remove from discovered_pub_sub_
            discovered_pub_sub_.erase(guid);
            it = participants_.erase(it);
        } else {
            ++it;
        }
    }

    // Build result with activity status
    for (const auto& [guid, info] : participants_) {
        ParticipantInfo copy = info;
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

    // Track first/last seen for frequency calculation
    std::unordered_map<std::string, uint64_t> topic_first_seen;
    std::unordered_map<std::string, uint64_t> topic_last_seen;

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
            topic.lost_count += ep.lost_count;
            topic.heartbeat_count += ep.heartbeat_count;

            // Track timestamps for frequency
            if (ep.first_seen_us > 0) {
                auto it = topic_first_seen.find(ep.topic_name);
                if (it == topic_first_seen.end() || ep.first_seen_us < it->second) {
                    topic_first_seen[ep.topic_name] = ep.first_seen_us;
                }
            }
            if (ep.last_seen_us > 0) {
                topic_last_seen[ep.topic_name] = ep.last_seen_us;
            }
        }
    }

    // Calculate rates
    std::vector<TopicInfo> result;
    result.reserve(topic_map.size());
    for (auto& [name, info] : topic_map) {
        // Loss rate
        uint64_t total_packets = info.data_count + info.lost_count;
        if (total_packets > 0) {
            info.loss_rate = static_cast<double>(info.lost_count) / static_cast<double>(total_packets);
        }

        // Retransmit rate (NACK count as proxy for retransmissions)
        if (info.data_count > 0) {
            info.retransmit_rate = static_cast<double>(info.nack_count) / static_cast<double>(info.data_count);
        }

        // Send frequency (messages per second)
        auto first_it = topic_first_seen.find(name);
        auto last_it = topic_last_seen.find(name);
        if (first_it != topic_first_seen.end() && last_it != topic_last_seen.end()) {
            uint64_t duration_us = last_it->second - first_it->second;
            if (duration_us > 1000000 && info.data_count > 1) {  // At least 1 second and 2 messages
                double duration_sec = static_cast<double>(duration_us) / 1000000.0;
                info.send_frequency_hz = static_cast<double>(info.data_count) / duration_sec;
            }
        }

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

    // Debug: count endpoints by type
    int writer_count = 0;
    int reader_count = 0;
    int writers_with_topic = 0;
    int readers_with_topic = 0;

    // Collect all endpoints grouped by topic
    for (const auto& [guid, ep] : endpoints_) {
        if (ep.is_writer) writer_count++;
        if (ep.is_reader) reader_count++;
        if (ep.topic_name.empty()) continue;

        if (ep.is_writer) writers_with_topic++;
        if (ep.is_reader) readers_with_topic++;

        auto& topic = topic_map[ep.topic_name];
        topic.topic_name = ep.topic_name;
        if (!ep.type_name.empty()) {
            topic.type_name = ep.type_name;
        }

        std::string participant_name = participant_names[ep.participant_guid];
        if (participant_name.empty()) {
            participant_name = "Unknown";
        }

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
