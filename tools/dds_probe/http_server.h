#pragma once

#include <string>
#include <thread>
#include <atomic>

#include "metrics_engine.h"
#include "pcap_worker.h"
#include "httplib.h"

namespace dds_probe {

// Small embedded HTTP/JSON front-end on top of cpp-httplib + nlohmann-json.
// Exposes the same live snapshot the TUI shows, as JSON, so an external client
// can query monitor state without driving the terminal UI.
//
// Threading: all handlers call MetricsEngine's read-only query APIs (which
// take their own shared_lock) and PcapWorker's capture-stats getters, so the
// HTTP thread needs no additional locking. The server runs in its own thread
// and is stopped on shutdown.
class HttpServer {
public:
    HttpServer(MetricsEngine& engine, PcapWorker& worker);

    // Bind and start the listening thread on `port`. Returns false on bind
    // failure (e.g. port in use). Idempotent: a second Start is a no-op.
    bool Start(int port);
    void Stop();

    bool Running() const { return running_.load(); }

private:
    void RegisterRoutes();

    MetricsEngine& engine_;
    PcapWorker& worker_;

    httplib::Server svr_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> started_{false};
};

}  // namespace dds_probe

