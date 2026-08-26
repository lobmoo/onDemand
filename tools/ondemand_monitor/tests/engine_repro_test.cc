// Repro test: "monitor starts first, then a node starts -> participant visible
// but topic list empty".
//
// Feeds MetricsEngine the discovery sequence a fresh FastDDS node emits and
// asserts the named participant ends up with its announced topics attached.
//
// Scenario A (control): SPDP arrives before SEDP  -> expected to pass today.
// Scenario B (bug):     SEDP arrives before SPDP  -> expected to fail today.
// Scenario C (bug var): like B but SEDP carries no PID_PARTICIPANT_GUID.
//
// Exit code 0 = all scenarios behave correctly.

#include <cassert>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "metrics_engine.h"

using namespace ondemand_monitor;

namespace {

constexpr uint8_t kDomId = 10;

// Synthetic node GUID prefix (12 bytes)
std::array<uint8_t, 12> NodePrefix(uint8_t seed) {
    std::array<uint8_t, 12> p;
    for (int i = 0; i < 12; ++i) p[i] = static_cast<uint8_t>(seed * 16 + i);
    return p;
}

GUID_t MakeGuid(const std::array<uint8_t, 12>& prefix, std::array<uint8_t, 4> eid) {
    GUID_t g;
    g.prefix = prefix;
    g.entityId = eid;
    return g;
}

const std::array<uint8_t, 4> kSpdpWriterEid = {0x00, 0x01, 0x00, 0xC2};   // builtin SPDP writer
const std::array<uint8_t, 4> kSedpPubWriterEid = {0x00, 0x00, 0x03, 0xC2}; // builtin SEDP publications writer
const std::array<uint8_t, 4> kParticipantEid = {0x00, 0x00, 0x01, 0xC1};   // participant entity id
const std::array<uint8_t, 4> kUserWriterEid = {0x00, 0x00, 0x03, 0x03};    // user data writer (bucket_0 pattern)

// SPDP announcement: domain + participant name (+ participant guid), no topic.
DataSubmessage MakeSpdp(const std::array<uint8_t, 12>& prefix,
                        const char* name = "pub_node") {
    DataSubmessage d;
    d.writer_guid = MakeGuid(prefix, kSpdpWriterEid);
    d.seq_num = {0, 1};
    d.has_domain_id = true;
    d.domain_id = kDomId;
    d.has_participant_name = true;
    d.participant_name = name;
    d.has_participant_guid = true;
    d.participant_guid = MakeGuid(prefix, kParticipantEid);
    return d;
}

// SEDP writer announcement: endpoint guid + topic/type + participant guid,
// NO participant name (FastDDS SEDP payloads carry no name parameter).
DataSubmessage MakeSedpWriter(const std::array<uint8_t, 12>& prefix) {
    DataSubmessage d;
    d.writer_guid = MakeGuid(prefix, kSedpPubWriterEid);  // sent by builtin pub writer
    d.seq_num = {0, 1};
    d.has_endpoint_guid = true;
    d.endpoint_guid = MakeGuid(prefix, kUserWriterEid);
    d.has_topic_name = true;
    d.topic_name = "dsf/sys/var/tableDefine";
    d.has_type_name = true;
    d.type_name = "PubTableDefine";
    d.has_participant_guid = true;
    d.participant_guid = MakeGuid(prefix, kParticipantEid);
    return d;
}

// Same as above but without PID_PARTICIPANT_GUID.
DataSubmessage MakeSedpWriterNoPGuid(const std::array<uint8_t, 12>& prefix) {
    DataSubmessage d = MakeSedpWriter(prefix);
    d.has_participant_guid = false;
    return d;
}

struct CheckResult {
    bool ok = false;
    std::string detail;
};

CheckResult CheckNamedParticipantHasTopic(MetricsEngine& engine,
                                          const char* name = "pub_node") {
    auto parts = engine.GetParticipants();
    const ParticipantInfo* named = nullptr;
    int named_count = 0;
    for (const auto& p : parts) {
        if (p.name == name) {
            named = &p;
            named_count++;
        }
    }
    if (named_count != 1) {
        return {false, "expected exactly 1 participant named '" +
                           std::string(name) + "', got " +
                           std::to_string(named_count)};
    }

    auto topics = engine.GetParticipantTopics(named->guid);
    bool has_topic = false;
    for (const auto& t : topics) {
        if (t.topic_name.find("tableDefine") != std::string::npos) has_topic = true;
    }
    if (!has_topic) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "named participant has %zu topic(s), expected tableDefine "
                 "(endpoints_count=%u)",
                 topics.size(), named->endpoints_count);
        return {false, buf};
    }
    return {true, "ok"};
}

void RunScenario(const char* label, uint64_t ts,
                 const std::function<void(MetricsEngine&, uint64_t)>& feed,
                 const char* name = "pub_node") {
    MetricsEngine engine;
    feed(engine, ts);

    // Dump internal state as observed through the public API.
    printf("[%s] --- state dump ---\n", label);
    auto parts = engine.GetParticipants();
    for (const auto& p : parts) {
        char g[64];
        snprintf(g, sizeof(g), "%02x..%02x:%02x%02x%02x%02x", p.guid.prefix[0],
                 p.guid.prefix[11], p.guid.entityId[0], p.guid.entityId[1],
                 p.guid.entityId[2], p.guid.entityId[3]);
        printf("  P %s name=[%s] dom=%u eps=%u\n", g, p.name.c_str(),
               p.domain_id, p.endpoints_count);
        for (const auto& ep : engine.GetEndpoints(p.guid)) {
            printf("      EP %02x%02x%02x%02x w=%d r=%d topic=[%s]\n",
                   ep.guid.entityId[0], ep.guid.entityId[1],
                   ep.guid.entityId[2], ep.guid.entityId[3],
                   (int)ep.is_writer, (int)ep.is_reader,
                   ep.topic_name.c_str());
        }
    }

    CheckResult r = CheckNamedParticipantHasTopic(engine, name);
    printf("[%s] %s%s%s\n", label, r.ok ? "PASS" : "FAIL", r.ok ? "" : " - ",
           r.detail.c_str());
}

}  // namespace

int main() {
    // Use realistic epoch-us timestamps: GetParticipants() cleans entries older
    // than 10s vs wall clock, so 1970-era stamps would be swept away.
    const uint64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

    RunScenario("A: SPDP -> SEDP", ts, [&](MetricsEngine& e, uint64_t t) {
        e.OnData(MakeSpdp(NodePrefix(1)), t);
        e.OnData(MakeSedpWriter(NodePrefix(1)), t + 1000);
    });

    RunScenario("B: SEDP -> SPDP", ts, [&](MetricsEngine& e, uint64_t t) {
        e.OnData(MakeSedpWriter(NodePrefix(2)), t);
        e.OnData(MakeSpdp(NodePrefix(2)), t + 1000);
    });

    RunScenario("C: SEDP(no pguid) -> SPDP", ts, [&](MetricsEngine& e, uint64_t t) {
        e.OnData(MakeSedpWriterNoPGuid(NodePrefix(3)), t);
        e.OnData(MakeSpdp(NodePrefix(3)), t + 1000);
    });

    // D: node name without pub/sub substring - the heuristic remount at
    // metrics_engine.cc (discovered_pub_sub_) must not be load-bearing.
    RunScenario("D: SEDP -> SPDP, neutral name", ts,
                [&](MetricsEngine& e, uint64_t t) {
                    e.OnData(MakeSedpWriter(NodePrefix(4)), t);
                    e.OnData(MakeSpdp(NodePrefix(4), "sensor_node"), t + 1000);
                },
                "sensor_node");

    // E: user DATA flows before any discovery, then SEDP, then SPDP.
    DataSubmessage data_pkt = MakeSedpWriter(NodePrefix(5));
    data_pkt.has_endpoint_guid = false;
    data_pkt.has_topic_name = false;
    data_pkt.has_type_name = false;
    data_pkt.has_participant_guid = false;
    data_pkt.seq_num = {0, 1};
    data_pkt.payload_size = 100;

    RunScenario("E: DATA -> SEDP -> SPDP", ts,
                [&](MetricsEngine& e, uint64_t t) {
                    e.OnData(data_pkt, t);                       // raw data first
                    e.OnData(MakeSedpWriter(NodePrefix(5)), t + 1000);
                    e.OnData(MakeSpdp(NodePrefix(5)), t + 2000);
                });

    return 0;
}
