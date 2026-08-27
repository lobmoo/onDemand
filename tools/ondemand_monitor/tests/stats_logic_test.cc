// Statistics-logic unit tests for MetricsEngine: fragmentation accounting,
// retransmission detection and loss classification.
//
// All cases feed explicit microsecond timestamps, so they are fully
// deterministic (no wall-clock dependence except the live-mode eviction case,
// whose outcome is stable by construction: an ancient packet timestamp always
// loses against the current wall clock).
//
// Exit code 0 = all cases pass.

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "metrics_engine.h"

using namespace ondemand_monitor;

namespace {

int g_failures = 0;

#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        auto _a = (actual);                                                   \
        auto _e = (expected);                                                 \
        if (!(_a == _e)) {                                                    \
            printf("  FAIL %s:%d: %s == %s (got %lld, want %lld)\n", __FILE__,\
                   __LINE__, #actual, #expected, (long long)(_a),            \
                   (long long)(_e));                                          \
            g_failures++;                                                     \
        }                                                                     \
    } while (0)

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

SequenceNumber_t MakeSeq(uint64_t sn) {
    SequenceNumber_t s;
    s.high = static_cast<int32_t>(sn >> 32);
    s.low = static_cast<uint32_t>(sn & 0xFFFFFFFFu);
    return s;
}

// Regular user data writer (bucket pattern)
DataSubmessage MakeUserData(const GUID_t& writer, uint64_t sn,
                            uint32_t payload_size = 100) {
    DataSubmessage d;
    d.writer_guid = writer;
    d.reader_guid.entityId = {0, 0, 0, 0};  // multicast: ENTITYID_UNKNOWN
    d.seq_num = MakeSeq(sn);
    d.payload_size = payload_size;
    return d;
}

FragSubmessage MakeFrag(const GUID_t& writer, uint64_t sn, uint32_t frag_start,
                        uint16_t frag_count, uint32_t payload_size = 10000) {
    FragSubmessage f;
    f.writer_guid = writer;
    f.seq_num = MakeSeq(sn);
    f.frag_start = frag_start;
    f.frag_count = frag_count;
    f.frag_size = 65000;
    f.payload_size = payload_size;
    return f;
}

EndpointInfo* FindEndpoint(std::vector<EndpointInfo>& eps, const GUID_t& guid) {
    for (auto& ep : eps) {
        if (ep.guid == guid) return &ep;
    }
    return nullptr;
}

// --- Case 1: an unfilled gap ages into LOSS, never into a retransmission ---
void TestGapTimeoutIsLoss() {
    printf("[1] gap timeout -> lost (low rate)\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto w = MakeGuid(NodePrefix(1), {0x00, 0x00, 0x03, 0x03});

    const uint64_t t0 = 10'000'000'000ull;
    e.OnData(MakeUserData(w, 1), t0);
    e.OnData(MakeUserData(w, 2), t0 + 1'000'000);
    e.OnData(MakeUserData(w, 5), t0 + 2'000'000);   // gaps {3,4} born here
    e.OnData(MakeUserData(w, 6), t0 + 8'000'000);   // both now older than 5s

    auto eps = e.GetEndpoints(e.GetParticipants()[0].guid);
    auto* ep = FindEndpoint(eps, w);
    assert(ep != nullptr);
    CHECK_EQ(ep->lost_count, 2u);
    CHECK_EQ(ep->retransmit_count, 0u);
}

// --- Case 2: HIGH-RATE regression — gaps far behind the SN head used to be ---
// swept into retransmit_count ("assume filled") before the 5s timeout could
// classify them. They must now be classified as loss.
void TestHighRateStaleGapsAreNotRetransmits() {
    printf("[2] stale gaps at high rate -> lost, not retransmit\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto w = MakeGuid(NodePrefix(2), {0x00, 0x00, 0x04, 0x03});

    const uint64_t t0 = 20'000'000'000ull;
    e.OnData(MakeUserData(w, 1), t0);
    e.OnData(MakeUserData(w, 3000), t0 + 100'000);      // gaps 2..2999
    e.OnData(MakeUserData(w, 3001), t0 + 6'000'000);    // all gaps now >5s old

    auto eps = e.GetEndpoints(e.GetParticipants()[0].guid);
    auto* ep = FindEndpoint(eps, w);
    assert(ep != nullptr);
    CHECK_EQ(ep->lost_count, 2998u);
    CHECK_EQ(ep->retransmit_count, 0u);
}

// --- Case 3: a gap filled by an arriving packet IS a retransmission ---
void TestGapFillIsRetransmit() {
    printf("[3] filled gap -> retransmit\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto w = MakeGuid(NodePrefix(3), {0x00, 0x00, 0x03, 0x03});

    const uint64_t t0 = 30'000'000'000ull;
    e.OnData(MakeUserData(w, 1), t0);
    e.OnData(MakeUserData(w, 4), t0 + 10'000);    // gaps {2,3}
    e.OnData(MakeUserData(w, 2), t0 + 100'000);   // fills gap 2
    e.OnData(MakeUserData(w, 3), t0 + 150'000);   // fills gap 3

    auto eps = e.GetEndpoints(e.GetParticipants()[0].guid);
    auto* ep = FindEndpoint(eps, w);
    assert(ep != nullptr);
    CHECK_EQ(ep->retransmit_count, 2u);
    CHECK_EQ(ep->lost_count, 0u);
}

// --- Case 4: NACK -> resend credits the requesting reader ---
void TestNackCreditOnResend() {
    printf("[4] NACKed SN resend credits requester\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto w = MakeGuid(NodePrefix(4), {0x00, 0x00, 0x03, 0x03});
    auto r = MakeGuid(NodePrefix(5), {0x00, 0x00, 0x03, 0x04});

    const uint64_t t0 = 40'000'000'000ull;
    e.OnData(MakeUserData(w, 1), t0);              // monitor sees SN1 only

    // Reader NACKs SN2 (reversed-bit mapping: bit 31 of word 0 = base+1)
    AcknackSubmessage ack;
    ack.writer_guid = w;
    ack.reader_guid = r;
    ack.reader_sn_state_base = MakeSeq(1);
    ack.reader_sn_state_bitmap = {0x80000000u};
    ack.final_flag = false;
    e.OnAcknack(ack, t0 + 50'000);

    CHECK_EQ(e.GetSummary().total_nacks, 1u);

    e.OnData(MakeUserData(w, 2), t0 + 200'000);    // the resend arrives

    // Both sides of the exchange must show the retransmission.
    uint64_t req_ret = 0, w_ret = 0;
    bool found_req = false, found_w = false;
    for (auto& p : e.GetParticipants()) {
        for (auto& ep : e.GetEndpoints(p.guid)) {
            if (ep.guid == r) { req_ret = ep.retransmit_count; found_req = true; }
            if (ep.guid == w) { w_ret = ep.retransmit_count; found_w = true; }
        }
    }
    CHECK_EQ(static_cast<int>(found_req), 1);
    CHECK_EQ(static_cast<int>(found_w), 1);
    CHECK_EQ(req_ret, 1u);
    CHECK_EQ(w_ret, 1u);
}

// --- Case 5: slow multi-fragment transmission is NOT a retransmission ---
void TestFragmentContinuation() {
    printf("[5] fragment continuation vs retransmit boundary\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto w = MakeGuid(NodePrefix(8), {0x00, 0x00, 0x05, 0x03});  // bucket_2

    const uint64_t t0 = 50'000'000'000ull;
    // Same sample (SN10) split across fragments arriving 60ms apart — beyond
    // kDupGuardUs (55ms), so without continuation tracking each fragment would
    // count as a "repeat burst" retransmission.
    e.OnFragment(MakeFrag(w, 10, 1, 1), t0);
    e.OnFragment(MakeFrag(w, 10, 2, 1), t0 + 60'000);
    e.OnFragment(MakeFrag(w, 10, 3, 1), t0 + 120'000);

    auto parts = e.GetParticipants();
    uint64_t ret = 99, frags = 0;
    std::string topic;
    bool found = false;
    for (auto& p : parts) {
        for (auto& ep : e.GetEndpoints(p.guid)) {
            if (ep.guid == w) {
                found = true;
                ret = ep.retransmit_count;
                frags = ep.frag_count;
                topic = ep.topic_name;
            }
        }
    }
    assert(found);
    CHECK_EQ(ret, 0u);
    CHECK_EQ(frags, 3u);
    CHECK_EQ(static_cast<int>(topic == "dsf/var/data/transfer/bucket_2"), 1);

    // Boundary: a fragment arriving AFTER the continuation window still counts
    // as a repeat (= retransmission).
    e.OnFragment(MakeFrag(w, 10, 4, 1), t0 + 700'000);
    parts = e.GetParticipants();
    for (auto& p : parts) {
        for (auto& ep : e.GetEndpoints(p.guid)) {
            if (ep.guid == w) ret = ep.retransmit_count;
        }
    }
    CHECK_EQ(ret, 1u);
}

// --- Case 6: builtin discovery DATA_FRAG must not create endpoints ---
void TestBuiltinFragmentExcluded() {
    printf("[6] builtin discovery fragment excluded\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto spdp_writer = MakeGuid(NodePrefix(9), {0x00, 0x01, 0x00, 0xC2});
    e.OnFragment(MakeFrag(spdp_writer, 1, 1, 1), 60'000'000'000ull);
    CHECK_EQ(e.GetSummary().total_endpoints, 0u);
}

// --- Case 7: offline replay must age participants in packet-time domain ---
void TestOfflineClockDomain() {
    printf("[7] offline clock domain survival / live-mode eviction\n");
    const uint64_t ancient = 1'000'000'000'000'000ull;  // long past

    // Offline: file timestamps compared among themselves -> stays alive
    MetricsEngine e_off;
    e_off.SetOfflineMode(true);
    auto w = MakeGuid(NodePrefix(10), {0x00, 0x00, 0x03, 0x03});
    DataSubmessage spdp = MakeUserData(w, 1);
    spdp.writer_guid = MakeGuid(NodePrefix(10), {0x00, 0x01, 0x00, 0xC2});
    spdp.has_domain_id = true;
    spdp.domain_id = 10;
    spdp.has_participant_name = true;
    spdp.participant_name = "old_node";
    e_off.OnData(spdp, ancient);
    CHECK_EQ(e_off.GetParticipants().size(), 1u);
    // Feed more traffic 30s later inside the capture's own timeline
    e_off.OnData(spdp, ancient + 30'000'000ull);
    CHECK_EQ(e_off.GetParticipants().size(), 1u);

    // Live: ancient packet timestamps lose against the wall clock -> evicted
    MetricsEngine e_live;
    e_live.SetOfflineMode(false);
    e_live.OnData(spdp, ancient);
    CHECK_EQ(e_live.GetParticipants().size(), 0u);
}

// --- Case 8: SEDP-before-SPDP with a neutral node name (no pub/sub hint) ---
// The endpoint must be adopted by the named participant and the placeholder
// ghost must disappear.
void TestNeutralNameAdoption() {
    printf("[8] neutral-name SEDP->SPDP adoption\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto prefix = NodePrefix(11);
    auto sedp_writer = MakeGuid(prefix, {0x00, 0x00, 0x03, 0xC2});
    auto user_writer = MakeGuid(prefix, {0x00, 0x00, 0x03, 0x03});

    const uint64_t t0 = 70'000'000'000ull;
    DataSubmessage sedp;
    sedp.writer_guid = sedp_writer;
    sedp.has_endpoint_guid = true;
    sedp.endpoint_guid = user_writer;
    sedp.has_topic_name = true;
    sedp.topic_name = "dsf/sys/var/tableDefine";
    sedp.has_type_name = true;
    sedp.type_name = "PubTableDefine";
    e.OnData(sedp, t0);

    DataSubmessage spdp;
    spdp.writer_guid = MakeGuid(prefix, {0x00, 0x01, 0x00, 0xC2});
    spdp.has_domain_id = true;
    spdp.domain_id = 10;
    spdp.has_participant_name = true;
    spdp.participant_name = "sensor_node";
    e.OnData(spdp, t0 + 1000);

    auto parts = e.GetParticipants();
    int named = 0;
    bool has_topic = false;
    for (auto& p : parts) {
        if (p.name == "sensor_node") {
            named++;
            for (auto& t : e.GetParticipantTopics(p.guid)) {
                if (t.topic_name.find("tableDefine") != std::string::npos)
                    has_topic = true;
            }
        }
    }
    CHECK_EQ(named, 1);
    CHECK_EQ(static_cast<int>(has_topic), 1);
    // No nameless ghost participant may survive the adoption
    CHECK_EQ(parts.size(), 1u);
}

// --- Case 9: implausible SN jumps must not materialize millions of gaps ---
void TestSnJumpBound() {
    printf("[9] SN jump sanity bound\n");
    MetricsEngine e;
    e.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto w = MakeGuid(NodePrefix(12), {0x00, 0x00, 0x03, 0x03});

    const uint64_t t0 = 80'000'000'000ull;
    e.OnData(MakeUserData(w, 1), t0);
    e.OnData(MakeUserData(w, 5'000'000'000ull), t0 + 10'000);  // absurd jump
    e.OnData(MakeUserData(w, 5'000'000'001ull), t0 + 20'000);  // stream resumes

    // Plausible mid-size jumps are still tracked as gaps
    MetricsEngine e2;
    e2.SetOfflineMode(true);  // deterministic: age against fed timestamps
    auto w2 = MakeGuid(NodePrefix(13), {0x00, 0x00, 0x03, 0x03});
    e2.OnData(MakeUserData(w2, 1), t0);
    e2.OnData(MakeUserData(w2, 10), t0 + 10'000);       // gaps 2..9
    e2.OnData(MakeUserData(w2, 11), t0 + 7'000'000);    // all age out
    auto parts = e2.GetParticipants();
    uint64_t lost = 0;
    for (auto& p : parts) {
        for (auto& ep : e2.GetEndpoints(p.guid)) {
            if (ep.guid == w2) lost = ep.lost_count;
        }
    }
    CHECK_EQ(lost, 8u);
}

}  // namespace

int main() {
    TestGapTimeoutIsLoss();
    TestHighRateStaleGapsAreNotRetransmits();
    TestGapFillIsRetransmit();
    TestNackCreditOnResend();
    TestFragmentContinuation();
    TestBuiltinFragmentExcluded();
    TestOfflineClockDomain();
    TestNeutralNameAdoption();
    TestSnJumpBound();

    if (g_failures == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
