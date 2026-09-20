#include "http_server.h"

#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

#include "httplib.h"
#include "nlohmann/json.hpp"

namespace ondemand_monitor {
namespace {

std::string FormatGuid(const GUID_t& g) {
    char buf[48];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                  g.prefix[0], g.prefix[1], g.prefix[2], g.prefix[3],
                  g.prefix[4], g.prefix[5], g.prefix[6], g.prefix[7],
                  g.prefix[8], g.prefix[9], g.prefix[10], g.prefix[11],
                  g.entityId[0], g.entityId[1], g.entityId[2], g.entityId[3]);
    return std::string(buf);
}

nlohmann::json ParticipantJson(const ParticipantInfo& p) {
    nlohmann::json j;
    j["guid"] = FormatGuid(p.guid);
    j["name"] = p.name;
    j["domain_id"] = p.domain_id;
    j["src_ip"] = p.src_ip;
    j["multicast_ip"] = p.multicast_ip;
    j["endpoints"] = p.endpoints_count;
    j["active"] = p.is_active;
    j["first_seen_us"] = p.first_seen_us;
    j["last_seen_us"] = p.last_seen_us;
    j["heartbeats"] = p.heartbeat_count;
    j["acknacks"] = p.acknack_count;
    j["nacks"] = p.nack_count;
    return j;
}

nlohmann::json TopicJson(const MetricsEngine::TopicInfo& t) {
    nlohmann::json j;
    j["topic_name"] = t.topic_name;
    j["type_name"] = t.type_name;
    std::string role = "R";
    if (t.has_writer && t.has_reader) role = "W+R";
    else if (t.has_writer) role = "W";
    j["role"] = role;
    j["data_count"] = t.data_count;
    j["bytes_sent"] = t.bytes_sent;
    j["frag_count"] = t.frag_count;
    j["ack_count"] = t.ack_count;
    j["nack_count"] = t.nack_count;
    j["unique_nacked_sn"] = t.unique_nacked_sn;
    j["fulfilled_nack_sn"] = t.fulfilled_nack_sn;
    j["unfulfilled_nack_sn"] = t.unfulfilled_nack_sn;
    j["lost_count"] = t.lost_count;
    j["loss_rate"] = t.loss_rate;
    j["retransmit_count"] = t.retransmit_count;
    j["retransmit_rate"] = t.retransmit_rate;
    j["send_frequency_hz"] = t.send_frequency_hz;
    return j;
}

void Reply(httplib::Response& res, const nlohmann::json& j) {
    res.set_content(j.dump(2), "application/json");
}

}  // namespace

HttpServer::HttpServer(MetricsEngine& engine, PcapWorker& worker)
    : engine_(engine), worker_(worker) {}

void HttpServer::RegisterRoutes() {
    svr_.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            "onDemand Monitor HTTP endpoint. See /api/participants, "
            "/api/topics, /api/match, /api/summary, /api/capture.\n",
            "text/plain");
    });

    svr_.Get("/api/summary",
             [this](const httplib::Request&, httplib::Response& res) {
                 auto s = engine_.GetSummary();
                 nlohmann::json j;
                 j["version"] = "1.0";
                 j["total_participants"] = s.total_participants;
                 j["total_endpoints"] = s.total_endpoints;
                 j["total_data_messages"] = s.total_data_messages;
                 j["total_bytes"] = s.total_bytes;
                 j["total_heartbeats"] = s.total_heartbeats;
                 j["total_acknacks"] = s.total_acknacks;
                 j["total_nacks"] = s.total_nacks;
                 Reply(res, j);
             });

    // First-hop query: all participants (list level for the picker UI).
    svr_.Get("/api/participants",
             [this](const httplib::Request&, httplib::Response& res) {
                 nlohmann::json j;
                 j["participants"] = nlohmann::json::array();
                 for (const auto& p : engine_.GetParticipants()) {
                     j["participants"].push_back(ParticipantJson(p));
                 }
                 Reply(res, j);
             });

    // Per-node detail + its topics.
    svr_.Get(R"(/api/participants/(.+))",
             [this](const httplib::Request& req, httplib::Response& res) {
                 const std::string guid_key = req.matches[1];
                 for (const auto& p : engine_.GetParticipants()) {
                     if (FormatGuid(p.guid) != guid_key) continue;
                     nlohmann::json j = ParticipantJson(p);
                     j["topics"] = nlohmann::json::array();
                     for (const auto& t : engine_.GetParticipantTopics(p.guid)) {
                         j["topics"].push_back(TopicJson(t));
                     }
                     Reply(res, j);
                     return;
                 }
                 nlohmann::json err;
                 err["error"] = "participant not found";
                 res.status = 404;
                 Reply(res, err);
             });

    svr_.Get("/api/topics",
             [this](const httplib::Request&, httplib::Response& res) {
                 // Global aggregate: union over all participants of the topics
                 // each publishes/subscribes, with match participants attached.
                 nlohmann::json j;
                 j["topics"] = nlohmann::json::array();
                 auto matches = engine_.GetAllTopicMatches();
                 nlohmann::json index_by_topic;
                 for (const auto& m : matches) {
                     index_by_topic[m.topic_name] = {
                         {"matched", m.is_matched},
                         {"writers", m.writer_participants},
                         {"readers", m.reader_participants}};
                 }
                 std::vector<std::string> seen;
                 for (const auto& p : engine_.GetParticipants()) {
                     for (const auto& t : engine_.GetParticipantTopics(p.guid)) {
                         if (std::find(seen.begin(), seen.end(), t.topic_name) !=
                             seen.end())
                             continue;
                         seen.push_back(t.topic_name);
                         nlohmann::json tj = TopicJson(t);
                         if (index_by_topic.contains(t.topic_name)) {
                             tj["matched"] =
                                 index_by_topic[t.topic_name]["matched"];
                             tj["writers"] =
                                 index_by_topic[t.topic_name]["writers"];
                             tj["readers"] =
                                 index_by_topic[t.topic_name]["readers"];
                         } else {
                             tj["matched"] = false;
                             tj["writers"] = nlohmann::json::array();
                             tj["readers"] = nlohmann::json::array();
                         }
                         j["topics"].push_back(tj);
                     }
                 }
                 Reply(res, j);
             });

    svr_.Get("/api/match",
             [this](const httplib::Request&, httplib::Response& res) {
                 nlohmann::json j;
                 j["matches"] = nlohmann::json::array();
                 for (const auto& m : engine_.GetAllTopicMatches()) {
                     j["matches"].push_back({
                         {"topic_name", m.topic_name},
                         {"matched", m.is_matched},
                         {"data_count", m.data_count},
                         {"writers", m.writer_participants},
                         {"readers", m.reader_participants},
                     });
                 }
                 Reply(res, j);
             });

    svr_.Get("/api/capture",
             [this](const httplib::Request&, httplib::Response& res) {
                 auto c = worker_.GetCaptureStats();
                 auto pc = worker_.GetPcapStats();
                 nlohmann::json j;
                 j["kernel_captured"] = c.captured;
                 j["kernel_dropped"] = c.kernel_dropped;
                 j["enqueued"] = c.enqueued;
                 j["enqueue_dropped"] = c.enqueue_dropped;
                 j["malformed"] = c.malformed;
                 j["queue_packets"] = c.queue_packets;
                 j["queue_bytes"] = c.queue_bytes;
                 j["iface_dropped"] = pc.iface_dropped;
                 Reply(res, j);
             });

    // One-request aggregate: everything a client needs in a single GET.
    svr_.Get("/api/full",
             [this](const httplib::Request&, httplib::Response& res) {
                 auto s = engine_.GetSummary();
                 auto matches = engine_.GetAllTopicMatches();
                 auto c = worker_.GetCaptureStats();
                 auto pc = worker_.GetPcapStats();

                 nlohmann::json j;
                 j["version"] = "1.0";
                 j["summary"] = {
                     {"total_participants", s.total_participants},
                     {"total_endpoints", s.total_endpoints},
                     {"total_data_messages", s.total_data_messages},
                     {"total_bytes", s.total_bytes},
                     {"total_heartbeats", s.total_heartbeats},
                     {"total_acknacks", s.total_acknacks},
                     {"total_nacks", s.total_nacks}};

                 j["participants"] = nlohmann::json::array();
                 for (const auto& p : engine_.GetParticipants()) {
                     nlohmann::json pj = ParticipantJson(p);
                     pj["topics"] = nlohmann::json::array();
                     for (const auto& t : engine_.GetParticipantTopics(p.guid)) {
                         pj["topics"].push_back(TopicJson(t));
                     }
                     j["participants"].push_back(std::move(pj));
                 }

                 j["matches"] = nlohmann::json::array();
                 for (const auto& m : matches) {
                     j["matches"].push_back({
                         {"topic_name", m.topic_name},
                         {"matched", m.is_matched},
                         {"data_count", m.data_count},
                         {"writers", m.writer_participants},
                         {"readers", m.reader_participants}});
                 }

                 j["capture"] = {
                     {"kernel_captured", c.captured},
                     {"kernel_dropped", c.kernel_dropped},
                     {"enqueued", c.enqueued},
                     {"enqueue_dropped", c.enqueue_dropped},
                     {"malformed", c.malformed},
                     {"queue_packets", c.queue_packets},
                     {"queue_bytes", c.queue_bytes},
                     {"iface_dropped", pc.iface_dropped}};
                 Reply(res, j);
             });

    svr_.set_exception_handler([](const httplib::Request&,
                                  httplib::Response& res,
                                  std::exception_ptr ep) {
        nlohmann::json err;
        err["error"] = "internal error";
        res.status = 500;
        res.set_content(err.dump(2), "application/json");
    });
}

bool HttpServer::Start(int port) {
    if (started_.exchange(true)) return true;
    RegisterRoutes();
    bool ok = svr_.bind_to_port("0.0.0.0", port);
    if (!ok) {
        started_.store(false);
        return false;
    }
    running_.store(true);
    thread_ = std::thread([this] { svr_.listen_after_bind(); });
    return true;
}

void HttpServer::Stop() {
    if (!started_.load()) return;
    running_.store(false);
    svr_.stop();
    if (thread_.joinable()) thread_.join();
    started_.store(false);
}

}  // namespace ondemand_monitor