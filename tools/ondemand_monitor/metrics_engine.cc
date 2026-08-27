#include "metrics_engine.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace ondemand_monitor {

// Activity threshold: 5 seconds (PDP-based)
static constexpr uint64_t ACTIVITY_THRESHOLD_US = 5 * 1000000ULL;
// Cleanup threshold: 10 seconds - remove inactive participants
static constexpr uint64_t CLEANUP_THRESHOLD_US = 10 * 1000000ULL;

// Minimum gap between two arrivals of the same SN for the repeat to count as a
// retransmission. `tcpdump -i any` delivers one copy per interface (lo/eth0/...)
// with measured spread up to ~20ms, so this must sit above that; real resends
// observed on the wire land 70ms+ later.
static constexpr int64_t kDupGuardUs = 55 * 1000;
// Window after a NACKed sample's retransmission arrived during which repeated
// (in-flight) ACKNACKs for the same SN are ignored — see EndpointInfo::recent_fulfilled.
static constexpr int64_t kFulfilledGuardUs = 500 * 1000;
// Upper bound of recent_sn_times per endpoint (SN window kept for repeat
// detection). Pruned lazily once exceeded.
static constexpr size_t kRecentSnCapacity = 4096;
// Max SN jump for which missing SNs are materialized as pending gaps. A
// corrupt parse or a writer restart can produce an enormous forward jump;
// per-SN bookkeeping for it would spin the gap-creation loop and poison loss
// stats with millions of phantom gaps. Beyond this bound the stream is treated
// as discontinuous: no gaps, last_sn simply advances.
static constexpr uint64_t kMaxGapJump = 65536;
// Max spacing between consecutive fragments of ONE sample (same writerSN,
// monotonically advancing fragmentStartingNum) for it to count as normal
// in-flight transmission rather than a retransmission. Trades a possible
// undercount of genuine fragment retransmits inside this window against the
// certain overcount of slow-but-normal multi-fragment samples (a 64KB sample
// at ~10Mbps takes >50ms — past kDupGuardUs).
static constexpr int64_t kFragContinuationUs = 500 * 1000;

// RTPS builtin discovery entities (SEDP writers/readers etc., well-known
// EntityIds like {00,00,03,C2}). Business requirement: the monitor only shows
// business-layer topics, so discovery traffic is EXCLUDED from stats entirely —
// no endpoints created, no ack/nack/retransmit counted for these entities.
// User endpoints in this system use EntityIds like {00,NN,xx,02/03/04}; RTPS
// builtin entities end in 0xC2/0xC7 (and some TXDDS ones in 0xC3).
static bool IsBuiltinDiscoveryEntity(const GUID_t& g) {
    return g.entityId[0] == 0x00 && g.entityId[1] == 0x00 &&
           (g.entityId[3] == 0xC2 || g.entityId[3] == 0xC7 || g.entityId[3] == 0xC3);
}

// SPDP builtin participant writer/reader EntityIds {00,01,00,C2}/{00,01,00,C7}.
// IsBuiltinDiscoveryEntity only covers the SEDP-style {00,00,x,Cx} pattern;
// the SPDP pair has entityId[1]==0x01 and needs its own check (used where no
// earlier SPDP branch has already filtered the traffic, e.g. OnFragment).
static bool IsSpdpEntity(const GUID_t& g) {
    return g.entityId[0] == 0x00 && g.entityId[1] == 0x01 &&
           (g.entityId[3] == 0xC2 || g.entityId[3] == 0xC7);
}

// Business topic names are derivable straight from the EntityId conventions of
// the OnDemand system. Naming an endpoint IMMEDIATELY on first traffic closes
// the window where a writer exists but is unnamed (its side then disappears
// from match computation and the topic shows "Unmatched" while data flows).
// EntityIds observed on the wire:
//   {00,00,01,02} tableDefine writer      {00,00,01,07} tableDefine reader
//   {00,00,02,03} register writer         {00,00,02,04} register reader
//   {00,00,N,03}  bucket_(N-3) writer     {00,00,N,04}  bucket_(N-3) reader
static std::string InferBusinessTopicName(const GUID_t& g) {
    if (g.entityId[0] != 0x00 || g.entityId[1] != 0x00) return "";
    switch (g.entityId[2]) {
        case 0x01:
            if (g.entityId[3] == 0x02 || g.entityId[3] == 0x07)
                return "dsf/sys/var/tableDefine";
            break;
        case 0x02:
            if (g.entityId[3] == 0x03 || g.entityId[3] == 0x04)
                return "dsf/message/commandRequest/subTableRegister";
            break;
        default:
            if (g.entityId[2] >= 0x03 &&
                (g.entityId[3] == 0x03 || g.entityId[3] == 0x04))
                return "dsf/var/data/transfer/bucket_" +
                       std::to_string(g.entityId[2] - 3);
            break;
    }
    return "";
}

void MetricsEngine::OnPacketSource(const std::array<uint8_t, 12>& src_prefix,
                                   const uint8_t* src_ip_be4, const uint8_t* dst_ip_be4) {
    std::unique_lock lock(mutex_);
    auto& addrs = prefix_addresses_[src_prefix];
    if (src_ip_be4) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 src_ip_be4[0], src_ip_be4[1], src_ip_be4[2], src_ip_be4[3]);
        addrs.first = buf;
    }
    // Class D range 224.0.0.0-239.255.255.255 = multicast group
    if (dst_ip_be4 && dst_ip_be4[0] >= 224 && dst_ip_be4[0] <= 239) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 dst_ip_be4[0], dst_ip_be4[1], dst_ip_be4[2], dst_ip_be4[3]);
        addrs.second = buf;
    }
}

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

    // Track latest timestamp for offline mode cleanup
    if (timestamp_us > latest_timestamp_us_) {
        latest_timestamp_us_ = timestamp_us;
    }

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

        // Retroactively adopt ANY endpoint created under this GUID prefix
        // before the participant was known — typically SEDP arriving ahead of
        // SPDP, where UpdateEndpoint fell back to the GetParticipantGuid
        // placeholder {..:000001c1}. The name-based remount below only helps
        // when the SEDP payload carried PID_PARTICIPANT_NAME (FastDDS does not
        // send it), so a neutrally-named node would otherwise keep a nameless
        // ghost participant holding all of its endpoints forever.
        {
            std::vector<GUID_t> adopt;
            for (auto& [ep_guid, ep] : endpoints_) {
                if (ep.participant_guid.prefix == participant_guid.prefix &&
                    !(ep.participant_guid == participant_guid)) {
                    adopt.push_back(ep_guid);
                }
            }
            for (const auto& ep_guid : adopt) {
                auto& ep = endpoints_[ep_guid];
                GUID_t old_key = ep.participant_guid;
                uint64_t moved_hb = 0, moved_ack = 0, moved_nack = 0;
                auto old_it = participants_.find(old_key);
                if (old_it != participants_.end()) {
                    auto& old_p = old_it->second;
                    if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                    // Preserve counters accumulated under the placeholder so
                    // adoption doesn't reset a node's visible history.
                    moved_hb = old_p.heartbeat_count;
                    moved_ack = old_p.acknack_count;
                    moved_nack = old_p.nack_count;
                    // Pure placeholder (no name, never saw PDP, no endpoints
                    // left) is an artifact, not a node — drop it instead of
                    // letting it linger in the node list for one clean cycle.
                    if (old_p.endpoints_count == 0 && old_p.name.empty() &&
                        old_p.last_pdp_seen_us == 0) {
                        participants_.erase(old_it);
                    }
                }
                ep.participant_guid = participant_guid;
                endpoint_to_participant_[ep_guid] = participant_guid;
                participant.heartbeat_count += moved_hb;
                participant.acknack_count += moved_ack;
                participant.nack_count += moved_nack;
                participant.endpoints_count++;
            }
        }

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

        // SEDP detection: has endpoint_guid (explicit), OR has topic_name/type_name
        // with participant_name (SEDP announcement without PID_ENDPOINT_GUID)
        bool is_sedp = data.has_endpoint_guid ||
                       (data.has_topic_name && data.has_participant_name);

        if (is_sedp) {
            // Determine the real endpoint GUID
            // If PID_ENDPOINT_GUID present, use it; otherwise use writer_guid
            GUID_t real_endpoint_guid = data.has_endpoint_guid ? data.endpoint_guid : data.writer_guid;

            // This is a SEDP discovery packet - handle endpoint creation
            if (data.has_participant_name && !data.participant_name.empty()) {
                sedp_endpoint_name_[real_endpoint_guid] = data.participant_name;
            }

            // Determine if this SEDP packet announces a writer or reader endpoint.
            // In FastDDS SEDP:
            // - publications writer (entityId {0x00,0x00,0x03,0xC2}) announces writer endpoints
            // - subscriptions writer (entityId {0x00,0x00,0x04,0xC2}) announces reader endpoints
            bool sedp_is_writer = true;  // Default to writer
            if (data.writer_guid.entityId[0] == 0x00 &&
                data.writer_guid.entityId[1] == 0x00 &&
                data.writer_guid.entityId[3] == 0xC2) {
                // Builtin SEDP writer - check entity ID to determine role
                // entityId[2]=0x03 = publications writer (announces writers)
                // entityId[2]=0x04 = subscriptions writer (announces readers)
                uint8_t sedp_entity_idx = data.writer_guid.entityId[2];
                sedp_is_writer = (sedp_entity_idx == 0x03);
            }

            // Create/update endpoint from SEDP (this has the correct GUID prefix)
            UpdateEndpoint(real_endpoint_guid, sedp_is_writer, timestamp_us);
            auto& sedp_endpoint = endpoints_[real_endpoint_guid];

            // Set topic/type from SEDP payload
            if (data.has_topic_name && !data.topic_name.empty()) {
                sedp_endpoint.topic_name = data.topic_name;
            }
            if (data.has_type_name && !data.type_name.empty()) {
                sedp_endpoint.type_name = data.type_name;
            }
            // Infer topic/type from entity ID if not set
            if (sedp_endpoint.topic_name.empty()) {
                uint8_t entity_idx = real_endpoint_guid.entityId[2];
                if (real_endpoint_guid.entityId[0] == 0x00 &&
                    real_endpoint_guid.entityId[1] == 0x00 &&
                    entity_idx >= 0x03) {
                    uint32_t bucket_idx = entity_idx - 3;
                    if (real_endpoint_guid.entityId[3] == 0x04) {
                        sedp_endpoint.is_reader = true;
                        sedp_endpoint.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket_idx);
                        sedp_endpoint.type_name = "TableDataTransfer";
                    } else if (real_endpoint_guid.entityId[3] == 0x03) {
                        sedp_endpoint.is_writer = true;
                        sedp_endpoint.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket_idx);
                        sedp_endpoint.type_name = "TableDataTransfer";
                    }
                }
            }

            // If still no role set, infer from writer_guid entity ID in the DATA submessage header
            if (!sedp_endpoint.is_writer && !sedp_endpoint.is_reader) {
                if (data.writer_guid.entityId[3] == 0x03) {
                    sedp_endpoint.is_writer = true;
                } else if (data.writer_guid.entityId[3] == 0x04) {
                    sedp_endpoint.is_reader = true;
                } else {
                    // Default: if topic_name present and came from writer_guid path, it's a writer
                    sedp_endpoint.is_writer = true;
                }
            }

            // Assign SEDP endpoint to participant
            // Try by name lookup first, then by GUID prefix
            bool assigned = false;
            if (data.has_participant_name && !data.participant_name.empty()) {
                auto name_it = name_to_participant_.find(data.participant_name);
                if (name_it != name_to_participant_.end()) {
                    endpoint_to_participant_[real_endpoint_guid] = name_it->second;
                    if (!(sedp_endpoint.participant_guid == name_it->second)) {
                        auto& old_p = participants_[sedp_endpoint.participant_guid];
                        if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                        sedp_endpoint.participant_guid = name_it->second;
                        participants_[name_it->second].endpoints_count++;
                    }
                    assigned = true;
                }
            }
            // Fallback: find participant by GUID prefix
            if (!assigned) {
                auto prefix_it = prefix_to_participant_.find(real_endpoint_guid.prefix);
                if (prefix_it != prefix_to_participant_.end()) {
                    endpoint_to_participant_[real_endpoint_guid] = prefix_it->second;
                    if (!(sedp_endpoint.participant_guid == prefix_it->second)) {
                        auto& old_p = participants_[sedp_endpoint.participant_guid];
                        if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                        sedp_endpoint.participant_guid = prefix_it->second;
                        participants_[prefix_it->second].endpoints_count++;
                    }
                    assigned = true;
                }
            }
            // If still not assigned, try participant_guid from PID_PARTICIPANT_GUID
            if (!assigned && data.has_participant_guid) {
                endpoint_to_participant_[real_endpoint_guid] = data.participant_guid;
                if (!(sedp_endpoint.participant_guid == data.participant_guid)) {
                    auto& old_p = participants_[sedp_endpoint.participant_guid];
                    if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                    sedp_endpoint.participant_guid = data.participant_guid;
                    participants_[data.participant_guid].endpoints_count++;
                }
                assigned = true;
            }
            // If still not assigned, store for retroactive assignment when SPDP arrives
            if (!assigned && data.has_participant_name && !data.participant_name.empty()) {
                // Will be assigned later in SPDP handler
            }

            // SEDP announcements carry sequence numbers too. Only track them on
            // an already-existing writer endpoint; discovery entities themselves
            // are excluded from stats (business-only view). Deliberately NOT
            // creating bare entries here — a bare entry would corrupt later SEDP
            // endpoint initialization (is_writer/participant_guid).
            auto w_it = endpoints_.find(data.writer_guid);
            if (w_it != endpoints_.end()) {
                TrackRetransmitAndGaps(w_it->second, data.seq_num, timestamp_us);
            }
            return;  // SEDP handled, skip data processing
        }

        // Regular data packet: track writer endpoints and reader stats

        // Business-only view: any builtin discovery entity that slipped past the
        // SPDP/SEDP branches is not tracked either
        if (IsBuiltinDiscoveryEntity(data.writer_guid)) {
            return;
        }

        // Find participant for writer using entity ID pattern
        bool is_data_writer = (data.writer_guid.entityId[0] == 0x00 &&
                               data.writer_guid.entityId[1] == 0x00 &&
                               data.writer_guid.entityId[3] == 0x03 &&
                               data.writer_guid.entityId[2] >= 0x03);

        // Zero-initialized explicitly: this used to be an uninitialized local,
        // and with no pub participant discovered yet the garbage prefix byte
        // skipped the fallback below, indexing participants_ under a random
        // key — one ghost participant leaked per packet.
        GUID_t writer_participant_guid{};
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

        // SEDP endpoint handling in data path: supplement topic info for endpoints
        // discovered via data packets (when SEDP was missed or has no endpoint_guid)
        if (data.has_topic_name && !data.topic_name.empty()) {
            // Determine the real endpoint GUID for this SEDP-like info
            GUID_t real_ep_guid = data.has_endpoint_guid ? data.endpoint_guid : data.writer_guid;

            // Update or create endpoint with topic info
            UpdateEndpoint(real_ep_guid, true, timestamp_us);
            auto& sedp_endpoint = endpoints_[real_ep_guid];
            sedp_endpoint.topic_name = data.topic_name;
            if (data.has_type_name && !data.type_name.empty()) {
                sedp_endpoint.type_name = data.type_name;
            }
            // Infer reader/writer from entity ID
            if (real_ep_guid.entityId[3] == 0x04) {
                sedp_endpoint.is_reader = true;
            } else if (real_ep_guid.entityId[3] == 0x03) {
                sedp_endpoint.is_writer = true;
            }

            // Assign endpoint to participant by name
            if (data.has_participant_name && !data.participant_name.empty()) {
                sedp_endpoint_name_[real_ep_guid] = data.participant_name;
                auto name_it = name_to_participant_.find(data.participant_name);
                if (name_it != name_to_participant_.end()) {
                    endpoint_to_participant_[real_ep_guid] = name_it->second;
                    if (!(sedp_endpoint.participant_guid == name_it->second)) {
                        auto& old_p = participants_[sedp_endpoint.participant_guid];
                        if (old_p.endpoints_count > 0) old_p.endpoints_count--;
                        sedp_endpoint.participant_guid = name_it->second;
                        participants_[name_it->second].endpoints_count++;
                    }
                }
            }
        }
        // NOTE: last_pdp_seen_us is ONLY updated by OnSPDP (true PDP discovery).
        // SEDP and data packets must NOT refresh it, otherwise participants
        // never become inactive when the demo stops.

        // Update writer endpoint with topic_name and type_name
        auto& endpoint = endpoints_[data.writer_guid];
        if (data.has_topic_name && !data.topic_name.empty()) {
            endpoint.topic_name = data.topic_name;
        }
        if (data.has_type_name && !data.type_name.empty()) {
            endpoint.type_name = data.type_name;
        }

        // Fallback: if no topic name from SEDP, infer from the EntityId
        // conventions (covers buckets AND tableDefine/register writers — an
        // unnamed endpoint would drop out of match computation and make an
        // actively-communicating topic show "Unmatched")
        if (endpoint.topic_name.empty()) {
            std::string inferred = InferBusinessTopicName(data.writer_guid);
            if (!inferred.empty()) {
                endpoint.topic_name = inferred;
                endpoint.type_name =
                    inferred.find("bucket_") != std::string::npos
                        ? "TableDataTransfer" : "";
            }
        }

        // Track reader stats: match by topic name.
        // OnDemand uses multicast (BEST_EFFORT), so DATA packets have
        // ENTITYID_UNKNOWN as reader_guid. We match by topic name instead.
        if (!endpoint.topic_name.empty()) {
            bool found_reader = false;
            uint64_t sn_u64 = data.seq_num.to_u64();
            for (auto& [ep_guid, ep] : endpoints_) {
                if (ep.is_reader && ep.topic_name == endpoint.topic_name) {
                    ep.data_count++;
                    ep.bytes_sent += data.payload_size;
                    ep.last_seen_us = timestamp_us;

                    // Track distinct bursts on the reader side too, using the
                    // same kDupGuardUs deduplication that writers use. Without
                    // this, `-i any` multi-interface copies inflate data_count
                    // and make the receive frequency appear N× too high.
                    bool distinct = false;
                    auto seen = ep.recent_sn_times.find(sn_u64);
                    if (seen != ep.recent_sn_times.end()) {
                        if ((int64_t)(timestamp_us - seen->second) > kDupGuardUs) {
                            seen->second = timestamp_us;
                            distinct = true;
                        }
                    } else {
                        ep.recent_sn_times.emplace(sn_u64, timestamp_us);
                        distinct = true;
                    }
                    if (distinct) {
                        ep.send_bursts++;
                        if (ep.first_burst_us == 0) ep.first_burst_us = timestamp_us;
                        ep.last_burst_us = timestamp_us;
                    }
                    // Lazy prune (same bound as writer path)
                    if (ep.recent_sn_times.size() > kRecentSnCapacity) {
                        for (auto sit = ep.recent_sn_times.begin();
                             sit != ep.recent_sn_times.end();) {
                            if (sit->first + kRecentSnCapacity < sn_u64)
                                sit = ep.recent_sn_times.erase(sit);
                            else
                                ++sit;
                        }
                    }

                    found_reader = true;
                }
            }
            // If no reader endpoint exists for this topic, create a virtual one.
            // In OnDemand multicast, SEDP only creates writer endpoints.
            // Reader endpoints are never announced via SEDP.
            // We create a virtual reader per topic to represent "subscribers are receiving".
            if (!found_reader) {
                // Use a synthetic GUID: same prefix as writer, entity ID based on topic hash
                GUID_t virtual_reader_guid;
                virtual_reader_guid.prefix = data.writer_guid.prefix;
                // Use a distinct entity ID pattern: 0000XX05 (virtual reader)
                uint8_t entity_idx = data.writer_guid.entityId[2];
                virtual_reader_guid.entityId[0] = 0x00;
                virtual_reader_guid.entityId[1] = 0x00;
                virtual_reader_guid.entityId[2] = entity_idx;
                virtual_reader_guid.entityId[3] = 0x05;  // virtual reader marker

                auto& virtual_reader = endpoints_[virtual_reader_guid];
                if (virtual_reader.topic_name.empty()) {
                    virtual_reader.guid = virtual_reader_guid;
                    virtual_reader.is_reader = true;
                    virtual_reader.topic_name = endpoint.topic_name;
                    virtual_reader.type_name = endpoint.type_name;
                    virtual_reader.first_seen_us = timestamp_us;
                    // Don't assign to writer's participant - virtual readers
                    // represent "all subscribers", not a specific sub node.
                    // Leave participant_guid as default (zeros).
                }
                virtual_reader.data_count++;
                virtual_reader.bytes_sent += data.payload_size;
                virtual_reader.last_seen_us = timestamp_us;
            }
        }

        // Update writer endpoint stats
        endpoint.data_count++;
        endpoint.bytes_sent += data.payload_size;

        // Timeout-based loss detection + NACK-based retransmit detection
        // (shared helper so DATA_FRAG and SEDP traffic get identical tracking)
        TrackRetransmitAndGaps(endpoint, data.seq_num, timestamp_us);
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

    // Track latest timestamp for offline mode cleanup
    if (timestamp_us > latest_timestamp_us_) {
        latest_timestamp_us_ = timestamp_us;
    }

    // Business-only view: discovery traffic (SEDP/SPDP entities) is not tracked
    if (IsBuiltinDiscoveryEntity(hb.writer_guid)) {
        return;
    }

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

    // Track latest timestamp for offline mode cleanup
    if (timestamp_us > latest_timestamp_us_) {
        latest_timestamp_us_ = timestamp_us;
    }

    // Business-only view: discovery traffic (SEDP/SPDP entities) is not tracked
    if (IsBuiltinDiscoveryEntity(ack.reader_guid) ||
        IsBuiltinDiscoveryEntity(ack.writer_guid)) {
        return;
    }

    UpdateEndpoint(ack.reader_guid, false, timestamp_us);
    UpdateEndpoint(ack.writer_guid, true, timestamp_us);
    // Name ACKNACK-created endpoints immediately from EntityId conventions so
    // they participate in topic matching without waiting for SEDP
    {
        auto& w_ep = endpoints_[ack.writer_guid];
        if (w_ep.topic_name.empty()) w_ep.topic_name = InferBusinessTopicName(ack.writer_guid);
        auto& r_ep = endpoints_[ack.reader_guid];
        if (r_ep.topic_name.empty()) r_ep.topic_name = InferBusinessTopicName(ack.reader_guid);
    }

    auto& endpoint = endpoints_[ack.reader_guid];
    endpoint.acknack_count++;

    // Count NACKs (bits set in bitmap) and record NACKed SNs.
    //
    // BIT ORDER (empirical, verified against HEARTBEAT ground truth in
    // dds_debug.pcap): this stack encodes the bitmap with the bit order
    // REVERSED inside each word — observed words are always top-aligned runs
    // whose popcount equals numBits, and the standard RTPS mapping
    // (bit j <-> base+1+j) would claim readers requested SN 28-33 while the
    // writer's own HEARTBEAT proved it had only sent up to SN 6. With reversed
    // bits the same packets decode to SN 2-7 / 2-10: a fresh reader missing the
    // head of the stream. So: bit b of word i corresponds to
    // base + 1 + i*32 + (31 - b).
    uint32_t nack_count = 0;
    uint64_t ack_base = ack.reader_sn_state_base.to_u64();
    // Writer endpoint accumulates both directions of the exchange: which SNs
    // were requested from it (nacked_requests, per requesting reader) and how
    // many requests it received (nack_count) — so its topic row shows the
    // demand side that explains its retransmit count.
    auto& writer_ep = endpoints_[ack.writer_guid];
    for (uint32_t word_idx = 0; word_idx < ack.reader_sn_state_bitmap.size(); ++word_idx) {
        uint32_t word = ack.reader_sn_state_bitmap[word_idx];
        nack_count += __builtin_popcount(word);
        // Record each NACKed SN on the writer endpoint with the requester
        if (word != 0) {
            for (int bit = 0; bit < 32; ++bit) {
                if (word & (1u << bit)) {
                    uint64_t nacked_sn =
                        ack_base + 1 + word_idx * 32 + (31 - bit);
                    // Suppress in-flight repeated requests for a sample that was
                    // just fulfilled — otherwise the resent packet's later
                    // interface copies hit the re-registered request and the
                    // retransmission is counted twice.
                    auto fulfilled_it = writer_ep.recent_fulfilled.find(nacked_sn);
                    if (fulfilled_it != writer_ep.recent_fulfilled.end() &&
                        (int64_t)(timestamp_us - fulfilled_it->second) < kFulfilledGuardUs) {
                        continue;
                    }
                    auto& requesters = writer_ep.nacked_requests[nacked_sn];
                    // Same reader repeating an unsatisfied request must not be
                    // double-credited when the resend arrives.
                    bool already = std::find(requesters.begin(), requesters.end(),
                                             ack.reader_guid) != requesters.end();
                    if (!already) {
                        requesters.push_back(ack.reader_guid);
                    }
                }
            }
        }
    }
    writer_ep.nack_count += nack_count;
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

void MetricsEngine::CheckPendingGaps(EndpointInfo& ep, uint64_t current_sn, uint64_t timestamp_us) {
    (void)current_sn;  // reserved for future SN-windowed cleanup

    // Check pending gaps for timeout.
    // Semantics: a gap that was never filled by an arriving packet is a LOSS,
    // full stop. The previous behavior additionally swept gaps "far behind"
    // the current SN into retransmit_count ("assume filled, we missed the
    // resend") — but at high rates the SN window slides past that threshold in
    // well under the 5s loss timeout, so every REAL loss was laundered into a
    // fake retransmission before it could ever be classified. An unfilled gap
    // now always ages into lost_count via the timeout branch below, which also
    // bounds pending_gaps memory to a 5s window on its own.
    auto it = ep.pending_gaps.begin();
    while (it != ep.pending_gaps.end()) {
        uint64_t gap_sn = it->first;
        uint64_t gap_time = it->second;

        // Gap older than LOSS_TIMEOUT_US (5 seconds) is confirmed loss
        if ((int64_t)(timestamp_us - gap_time) > (int64_t)LOSS_TIMEOUT_US) {
            ep.lost_count++;
            ep.nacked_requests.erase(gap_sn);  // Clean up NACK tracking
            it = ep.pending_gaps.erase(it);
        } else {
            ++it;
        }
    }

    // Fulfillment markers only need to outlive the in-flight NACK window
    for (auto fit = ep.recent_fulfilled.begin(); fit != ep.recent_fulfilled.end();) {
        if ((int64_t)(timestamp_us - fit->second) > 10 * (int64_t)kFulfilledGuardUs) {
            fit = ep.recent_fulfilled.erase(fit);
        } else {
            ++fit;
        }
    }
}

void MetricsEngine::TrackRetransmitAndGaps(EndpointInfo& ep, const SequenceNumber_t& seq,
                                            uint64_t timestamp_us) {
    uint64_t sn = seq.to_u64();
    uint64_t last_sn = ep.last_sn.to_u64();

    // Retransmit detection must run for ALL packets (SN <= last_sn included),
    // because retransmitted packets never advance last_sn.
    bool counted_as_retransmit = false;
    auto it = ep.pending_gaps.find(sn);
    if (it != ep.pending_gaps.end()) {
        // Gap was filled - packet was retransmitted successfully
        ep.pending_gaps.erase(it);
        ep.retransmit_count++;
        counted_as_retransmit = true;
    }

    // Also check if this SN was previously NACKed. Catches retransmissions that
    // arrive before a gap is recorded in pending_gaps (or when no gap exists at
    // all because only the reader missed the packet, not the monitor).
    auto nack_hit = ep.nacked_requests.find(sn);
    if (nack_hit != ep.nacked_requests.end()) {
        // Credit every reader that requested this sample — one multicast resend
        // fulfills all of them, and each requesting side should see its own
        // "retransmitted packets received" number.
        for (const auto& requester : nack_hit->second) {
            auto r_it = endpoints_.find(requester);
            if (r_it != endpoints_.end()) {
                r_it->second.retransmit_count++;
            }
        }
        ep.nacked_requests.erase(nack_hit);
        // Remember the fulfillment so repeated in-flight ACKNACKs for this SN
        // don't re-register it (see the insertion-side guard in OnAcknack).
        ep.recent_fulfilled[sn] = timestamp_us;
        if (!counted_as_retransmit) {
            ep.retransmit_count++;
        }
        // Mark as counted so the repeat-arrival check below doesn't charge the
        // SAME arriving packet twice (once via NACK match, once via repeat).
        counted_as_retransmit = true;
    }

    // Repeat-arrival bookkeeping: runs ALWAYS so recent_sn_times stays accurate
    // even when this packet was already counted via a gap/NACK match (otherwise
    // a SECOND resend of the same SN would find no baseline and be missed).
    // Counting is gated: a reliable writer may also resend samples with NO
    // monitor-visible gap and NO NACK — e.g. pushing history to a freshly
    // matched reader — and that repeat counts as a retransmit only when it
    // lands after kDupGuardUs: with `tcpdump -i any` every multicast packet is
    // captured once per interface (measured spread up to ~20ms), and those
    // copies must not count. SPDP is excluded entirely — its periodic announce
    // repeats SN=1 every second by design.
    bool is_spdp_announce =
        ep.guid.entityId[0] == 0x00 && ep.guid.entityId[1] == 0x01 &&
        ep.guid.entityId[2] == 0x00 && ep.guid.entityId[3] == 0xC2;
    bool is_repeat_burst = false;
    bool distinct_send = false;
    auto seen_it = ep.recent_sn_times.find(sn);
    if (seen_it != ep.recent_sn_times.end()) {
        if ((int64_t)(timestamp_us - seen_it->second) > kDupGuardUs) {
            seen_it->second = timestamp_us;  // a new distinct burst begins here
            is_repeat_burst = true;
            distinct_send = true;
        }
        // else: duplicate interface copy within the guard — ignore silently
    } else {
        ep.recent_sn_times.emplace(sn, timestamp_us);
        distinct_send = true;  // first arrival of this SN is a real send
    }
    if (distinct_send) {
        ep.send_bursts++;
        if (ep.first_burst_us == 0) {
            ep.first_burst_us = timestamp_us;
        }
        ep.last_burst_us = timestamp_us;
    }
    if (!is_spdp_announce && !counted_as_retransmit && is_repeat_burst) {
        ep.retransmit_count++;
    }
    // Lazy prune: keep the window bounded around the newest SN
    if (ep.recent_sn_times.size() > kRecentSnCapacity) {
        for (auto sit = ep.recent_sn_times.begin(); sit != ep.recent_sn_times.end();) {
            if (sit->first + kRecentSnCapacity < sn)
                sit = ep.recent_sn_times.erase(sit);
            else
                ++sit;
        }
    }

    if (last_sn > 0 && sn > last_sn + 1) {
        // SN jumped: mark missing SNs as pending gaps — but only for plausible
        // jumps (see kMaxGapJump). Beyond the bound this is a stream
        // discontinuity, not a burst of losses.
        if (sn - last_sn <= kMaxGapJump) {
            for (uint64_t missing_sn = last_sn + 1; missing_sn < sn; ++missing_sn) {
                // Only add if not already received (could happen with out-of-order)
                if (ep.pending_gaps.find(missing_sn) == ep.pending_gaps.end()) {
                    ep.pending_gaps[missing_sn] = timestamp_us;
                }
            }
        }
    }

    // Any gap older than LOSS_TIMEOUT_US (5 seconds) is confirmed loss
    CheckPendingGaps(ep, sn, timestamp_us);

    // Update last SN (only forward, never backward)
    if (sn > last_sn) {
        ep.last_sn = seq;
    }
}

void MetricsEngine::OnFragment(const FragSubmessage& frag, uint64_t timestamp_us) {
    std::unique_lock lock(mutex_);

    // Track latest timestamp for offline mode cleanup
    if (timestamp_us > latest_timestamp_us_) {
        latest_timestamp_us_ = timestamp_us;
    }

    // Business-only view: builtin discovery traffic must not create endpoints
    // here either — OnData/OnHeartbeat/OnAcknack all exclude these entities,
    // this path silently did not. Covers both the SEDP-style and the SPDP
    // EntityId patterns (there is no earlier SPDP branch on this path).
    if (IsBuiltinDiscoveryEntity(frag.writer_guid) || IsSpdpEntity(frag.writer_guid)) {
        return;
    }

    UpdateEndpoint(frag.writer_guid, true, timestamp_us);
    auto& endpoint = endpoints_[frag.writer_guid];

    // Name fragment-only endpoints immediately from EntityId conventions. A
    // large tableDefine travels exclusively as DATA_FRAG (>UDP payload limit);
    // without this it stayed unnamed until an unrelated DATA submessage arrived.
    if (endpoint.topic_name.empty()) {
        std::string inferred = InferBusinessTopicName(frag.writer_guid);
        if (!inferred.empty()) {
            endpoint.topic_name = inferred;
            endpoint.type_name =
                inferred.find("bucket_") != std::string::npos ? "TableDataTransfer" : "";
        }
    }

    endpoint.frag_count += frag.frag_count;
    endpoint.bytes_sent += frag.payload_size;

    uint64_t sn_u64 = frag.seq_num.to_u64();

    // Same-sample continuation: consecutive fragments share writerSN and
    // advance fragmentStartingNum. Refreshing that SN's burst baseline keeps
    // the repeat-burst heuristic (which fires after kDupGuardUs) from charging
    // a slow multi-fragment transmission as a retransmission. See
    // kFragContinuationUs for the trade-off.
    if (sn_u64 == endpoint.last_frag_sn &&
        frag.frag_start > endpoint.last_frag_start &&
        (int64_t)(timestamp_us - endpoint.last_frag_us) < kFragContinuationUs) {
        auto seen_it = endpoint.recent_sn_times.find(sn_u64);
        if (seen_it != endpoint.recent_sn_times.end()) {
            seen_it->second = timestamp_us;
        }
    }
    endpoint.last_frag_sn = sn_u64;
    endpoint.last_frag_start = frag.frag_start;
    endpoint.last_frag_us = timestamp_us;

    // Fragments share writerSN across submessages of the same sample, so the
    // same gap/retransmit logic applies unchanged (duplicate SNs of consecutive
    // fragments neither create gaps nor count as retransmits).
    TrackRetransmitAndGaps(endpoint, frag.seq_num, timestamp_us);

    GUID_t participant_guid = GetParticipantGuid(frag.writer_guid);
    auto& participant = participants_[participant_guid];
    participant.last_seen_us = timestamp_us;

    total_bytes_ += frag.payload_size;
}

std::vector<ParticipantInfo> MetricsEngine::GetParticipants() {
    std::unique_lock lock(mutex_);
    std::vector<ParticipantInfo> result;

    // Reference "now" for activity/cleanup, per mode:
    // - Live: wall clock (latest_timestamp_us_ freezes when traffic stops, so
    //   participants must age out against real time).
    // - Offline replay: the packet-time domain. File timestamps are compared
    //   against each other; comparing them to the wall clock would instantly
    //   evict every participant when replaying a capture older than 10s.
    uint64_t now = offline_mode_.load()
        ? latest_timestamp_us_
        : std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count();

    // Cleanup: remove participants inactive for more than 10 seconds.
    // Removals are collected first so auxiliary maps can be cascaded below
    // only when something was actually removed (this getter runs at UI frame
    // rate; steady state must stay cheap).
    std::vector<GUID_t> removed_participants;
    std::unordered_set<GUID_t, GUIDHash> removed_endpoints;
    auto it = participants_.begin();
    while (it != participants_.end()) {
        auto& [guid, info] = *it;
        bool should_remove = false;

        // Remove if inactive for too long (10 seconds since last PDP/SPDP).
        // Signed compare: packet timestamps can be slightly AHEAD of wall clock
        // (pcap kernel timestamps, offline replay); unsigned subtraction would
        // underflow and instantly delete perfectly fresh participants.
        if (info.last_pdp_seen_us > 0 &&
            (int64_t)(now - info.last_pdp_seen_us) > (int64_t)CLEANUP_THRESHOLD_US) {
            should_remove = true;
        }
        // Also remove if never got PDP/SPDP and inactive for 10 seconds since last seen
        if (info.last_pdp_seen_us == 0 && info.last_seen_us > 0 &&
            (int64_t)(now - info.last_seen_us) > (int64_t)CLEANUP_THRESHOLD_US) {
            should_remove = true;
        }

        if (should_remove) {
            removed_participants.push_back(guid);
            // Remove associated endpoints
            auto ep_it = endpoints_.begin();
            while (ep_it != endpoints_.end()) {
                bool owned = (ep_it->second.participant_guid == guid);
                // Virtual readers (entityId[3]==0x05) carry a zero
                // participant_guid but belong to their writer's GUID prefix;
                // tie their lifetime to the owning participant, otherwise they
                // linger forever after the node disappears.
                bool virtual_reader_of =
                    ep_it->second.is_reader &&
                    ep_it->second.participant_guid.prefix[0] == 0 &&
                    ep_it->first.entityId[3] == 0x05 &&
                    ep_it->first.prefix == guid.prefix;
                if (owned || virtual_reader_of) {
                    removed_endpoints.insert(ep_it->first);
                    ep_it = endpoints_.erase(ep_it);
                } else {
                    ++ep_it;
                }
            }
            it = participants_.erase(it);
        } else {
            ++it;
        }
    }

    // Cascade cleanup: every auxiliary map keyed by participant or endpoint
    // must drop entries whose owner is gone. Previously these survived forever,
    // so each participant churn cycle leaked rows and left stale mappings that
    // could resurrect wrong associations when GUID prefixes get reused.
    for (const auto& guid : removed_participants) {
        discovered_pub_sub_.erase(guid);
        prefix_addresses_.erase(guid.prefix);
        for (auto m = name_to_participant_.begin(); m != name_to_participant_.end();) {
            m = (m->second == guid) ? name_to_participant_.erase(m) : std::next(m);
        }
        for (auto m = prefix_to_participant_.begin(); m != prefix_to_participant_.end();) {
            m = (m->second == guid) ? prefix_to_participant_.erase(m) : std::next(m);
        }
        for (auto m = prefix_to_pub_sub_.begin(); m != prefix_to_pub_sub_.end();) {
            m = (m->second == guid) ? prefix_to_pub_sub_.erase(m) : std::next(m);
        }
    }
    if (!removed_endpoints.empty() || !removed_participants.empty()) {
        for (auto m = endpoint_to_participant_.begin();
             m != endpoint_to_participant_.end();) {
            bool dead_key = removed_endpoints.count(m->first) > 0;
            bool dead_val = std::find(removed_participants.begin(),
                                      removed_participants.end(), m->second) !=
                            removed_participants.end();
            m = (dead_key || dead_val) ? endpoint_to_participant_.erase(m) : std::next(m);
        }
        for (auto m = sedp_endpoint_name_.begin(); m != sedp_endpoint_name_.end();) {
            m = (removed_endpoints.count(m->first) > 0)
                    ? sedp_endpoint_name_.erase(m)
                    : std::next(m);
        }
        for (auto m = transfers_.begin(); m != transfers_.end();) {
            bool dead = removed_endpoints.count(m->first.writer) > 0 ||
                        removed_endpoints.count(m->first.reader) > 0;
            m = dead ? transfers_.erase(m) : std::next(m);
        }
    }

    // Build result with activity status
    for (const auto& [guid, info] : participants_) {
        ParticipantInfo copy = info;
        // Signed compare here too — see the underflow note above.
        copy.is_active = (info.last_pdp_seen_us > 0) &&
                         ((int64_t)(now - info.last_pdp_seen_us) < (int64_t)ACTIVITY_THRESHOLD_US);
        // Join observed network addresses (learned per packet, may predate SPDP)
        auto addr_it = prefix_addresses_.find(guid.prefix);
        if (addr_it != prefix_addresses_.end()) {
            copy.src_ip = addr_it->second.first;
            copy.multicast_ip = addr_it->second.second;
        }
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
    // Only count participants that have actual activity (not empty entries)
    size_t active_count = 0;
    for (const auto& [guid, info] : participants_) {
        if (info.last_seen_us > 0 || info.last_pdp_seen_us > 0) {
            active_count++;
        }
    }
    s.total_participants = active_count;
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

    // Distinct-send aggregation for accurate frequency: per topic we keep the
    // largest burst count seen on any single WRITER endpoint plus that
    // endpoint's first/last burst timestamps.
    struct BurstAgg {
        uint64_t bursts = 0;
        uint64_t first_us = 0;
        uint64_t last_us = 0;
    };
    std::unordered_map<std::string, BurstAgg> topic_bursts;

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
            topic.frag_count += ep.frag_count;
            topic.ack_count += ep.acknack_count;
            topic.nack_count += ep.nack_count;
            topic.lost_count += ep.lost_count;
            topic.retransmit_count += ep.retransmit_count;
            topic.heartbeat_count += ep.heartbeat_count;

            // Frequency source: the deduplicated burst counter (immune to
            // `-i any` multi-interface copies). Both writers and readers
            // accumulate send_bursts via TrackRetransmitAndGaps; writer data
            // wins when both exist on the same topic (it's the authoritative
            // transmit rate).
            if (ep.send_bursts > 0) {
                auto& agg = topic_bursts[ep.topic_name];
                if (ep.is_writer || agg.bursts == 0) {
                    if (ep.send_bursts > agg.bursts) {
                        agg.bursts = ep.send_bursts;
                        agg.first_us = ep.first_burst_us;
                        agg.last_us = ep.last_burst_us;
                    }
                }
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

        // Retransmit rate (actual retransmitted packets / total data packets)
        if (info.data_count > 0) {
            info.retransmit_rate = static_cast<double>(info.retransmit_count) / static_cast<double>(info.data_count);
        }

        // Send frequency via the burst estimator: for a periodic stream,
        // (bursts-1)/(first..last span) equals the true publish period exactly,
        // independent of interface duplicates and endpoint lifetime tails.
        auto b_it = topic_bursts.find(name);
        if (b_it != topic_bursts.end() && b_it->second.bursts >= 2 &&
            b_it->second.last_us > b_it->second.first_us) {
            double sec = static_cast<double>(b_it->second.last_us - b_it->second.first_us) / 1000000.0;
            if (sec > 0.5) {
                info.send_frequency_hz =
                    static_cast<double>(b_it->second.bursts - 1) / sec;
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
        if (!info.name.empty()) {
            participant_names[guid] = info.name;
        }
    }

    // Build topic→real reader participant mapping from SPDP-created reader endpoints.
    // Real readers have valid participant_guid (non-zero prefix).
    // Virtual readers (entityId[3]==0x05) have participant_guid=zeros.
    // We use this mapping to resolve virtual reader participant names.
    std::unordered_map<std::string, std::string> topic_to_reader_participant;
    for (const auto& [guid, ep] : endpoints_) {
        if (ep.is_reader && !ep.topic_name.empty() &&
            ep.participant_guid.prefix[0] != 0) {
            auto it = participant_names.find(ep.participant_guid);
            if (it != participant_names.end()) {
                topic_to_reader_participant[ep.topic_name] = it->second;
            }
        }
    }

    // Collect all endpoints grouped by topic
    for (const auto& [guid, ep] : endpoints_) {
        if (ep.topic_name.empty()) continue;

        auto& topic = topic_map[ep.topic_name];
        topic.topic_name = ep.topic_name;
        if (!ep.type_name.empty()) {
            topic.type_name = ep.type_name;
        }

        // Resolve participant name
        std::string participant_name;
        auto name_it = participant_names.find(ep.participant_guid);
        if (name_it != participant_names.end()) {
            participant_name = name_it->second;
        }
        // Prefix-level fallback: endpoints created before SPDP (or via the
        // GetParticipantGuid placeholder) carry a participant_guid whose 12-byte
        // prefix matches the real participant but whose EntityId differs, so the
        // exact-key lookup above fails and the endpoint silently dropped out of
        // matching — making live topics show "Unmatched".
        if (participant_name.empty()) {
            for (const auto& [guid, info] : participants_) {
                if (info.name.empty()) continue;
                if (std::equal(guid.prefix.begin(), guid.prefix.end(),
                               ep.participant_guid.prefix.begin())) {
                    participant_name = info.name;
                    break;
                }
            }
        }

        // For virtual readers (entityId[3]==0x05, participant_guid=zeros),
        // resolve from real reader endpoints on the same topic
        if (participant_name.empty() && ep.is_reader &&
            ep.guid.entityId[3] == 0x05) {
            auto reader_it = topic_to_reader_participant.find(ep.topic_name);
            if (reader_it != topic_to_reader_participant.end()) {
                participant_name = reader_it->second;
            }
        }

        // Skip endpoints with no resolvable participant
        if (participant_name.empty()) continue;

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
        // Standard matching: need both writer and reader
        bool has_both = !topic.writer_participants.empty() && !topic.reader_participants.empty();

        // Special topics: tableDefine and subTableRegister are discovery topics.
        // In OnDemand, these topics use multicast and SEDP may not always detect
        // reader endpoints (especially if SEDP parsing has issues).
        // For robustness, we treat these as matched if they have a writer.
        bool is_discovery_topic = (name.find("dsf/sys/var/tableDefine") != std::string::npos) ||
                                  (name.find("dsf/message/commandRequest/subTableRegister") != std::string::npos);

        if (is_discovery_topic) {
            // Discovery topics are matched if they have a writer
            topic.is_matched = !topic.writer_participants.empty();
        } else {
            // Regular topics need both writer and reader
            topic.is_matched = has_both;
        }
    }

    std::vector<TopicMatchInfo> result;
    result.reserve(topic_map.size());
    for (auto& [name, info] : topic_map) {
        result.push_back(std::move(info));
    }
    return result;
}

}  // namespace ondemand_monitor
