#include <iostream>
#include <string>
#include <cstring>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include "pcap_worker.h"
#include "rtps_parser.h"
#include "metrics_engine.h"
#include "match_analyzer.h"
#include "monitor_ui.h"

#include <algorithm>
#include <cstdio>
#include <thread>
#include <chrono>

namespace {
std::atomic<bool> g_running{true};

// Headless diagnostic: replay the pcap through the exact production pipeline
// (fragment reassembly -> RTPS parse -> MetricsEngine) and print how each topic
// is attributed. Lets us see which bucket a non-standard writer EntityId maps
// to without running the TUI.
void DumpResult(const ondemand_monitor::MetricsEngine& engine,
                const std::vector<ondemand_monitor::ParticipantInfo>& parts) {
    std::printf("\n=== PARTICPANTS ===\n");
    for (const auto& p : parts) {
        std::printf("participant name=['%s'] prefix=%02x%02x%02x%02x%02x%02x eps=%u\n",
                    p.name.c_str(),
                    p.guid.prefix[0], p.guid.prefix[1], p.guid.prefix[2],
                    p.guid.prefix[3], p.guid.prefix[4], p.guid.prefix[5],
                    p.endpoints_count);
        auto topics = engine.GetParticipantTopics(p.guid);
        for (const auto& t : topics) {
            std::printf("    topic=[%s] w=%d r=%d data=%llu frags=%llu bytes=%llu\n",
                        t.topic_name.c_str(), (int)t.has_writer, (int)t.has_reader,
                        (unsigned long long)t.data_count, (unsigned long long)t.frag_count,
                        (unsigned long long)t.bytes_sent);
        }
        for (const auto& ep : engine.GetEndpoints(p.guid)) {
            std::printf("    endpoint eid=%02x%02x%02x%02x role=%s topic=[%s] data=%llu\n",
                        ep.guid.entityId[0], ep.guid.entityId[1], ep.guid.entityId[2],
                        ep.guid.entityId[3], ep.is_writer ? "W" : "R",
                        ep.topic_name.c_str(), (unsigned long long)ep.data_count);
        }
    }
    std::printf("\n=== TOPIC MATCHES ===\n");
    for (const auto& m : engine.GetAllTopicMatches()) {
        std::printf("topic=[%s] data=%llu matched=%d writers=%zu readers=%zu\n",
                    m.topic_name.c_str(), (unsigned long long)m.data_count,
                    (int)m.is_matched, m.writer_participants.size(),
                    m.reader_participants.size());
    }
}

void DrainAndDump(ondemand_monitor::PcapWorker& worker,
                  ondemand_monitor::MetricsEngine& engine) {
    std::vector<ondemand_monitor::RawPacket> packets(1024);
    size_t guard = 0;
    while (guard++ < 1000000) {
        size_t count = worker.PopPackets(packets.data(), packets.size());
        if (count == 0) {
            if (worker.IsFinished()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        engine.BeginBatch();
        for (size_t i = 0; i < count; ++i) {
            ondemand_monitor::RtpsMessage msg;
            bool parsed = ondemand_monitor::RtpsParser::ParseHeader(
                packets[i].data.data(), packets[i].len, msg);
            if (!parsed) continue;
            engine.OnPacketSource(msg.source_guid_prefix,
                                  packets[i].src_ip, packets[i].dst_ip);
            struct Ctx { ondemand_monitor::MetricsEngine* e; uint64_t ts; uint16_t sp; uint16_t dp; };
            Ctx ctx{&engine, packets[i].timestamp_us, packets[i].src_port, packets[i].dst_port};
            ondemand_monitor::RtpsParser::ParseSubmessages(
                packets[i].data.data(), packets[i].len, msg.source_guid_prefix, &ctx,
                [](void* u, ondemand_monitor::DataSubmessage& d) {
                    auto* c = static_cast<Ctx*>(u);
                    c->e->OnData(d, c->ts);
                },
                [](void* u, const ondemand_monitor::HeartbeatSubmessage& hb) {
                    auto* c = static_cast<Ctx*>(u);
                    c->e->OnHeartbeat(hb, c->ts);
                },
                [](void* u, const ondemand_monitor::AcknackSubmessage& ack) {
                    auto* c = static_cast<Ctx*>(u);
                    c->e->OnAcknack(ack, c->ts);
                },
                [](void* u, ondemand_monitor::FragSubmessage& f) {
                    auto* c = static_cast<Ctx*>(u);
                    c->e->OnFragment(f, c->ts);
                });
        }
        engine.EndBatch();
    }
    DumpResult(engine, engine.GetParticipants());
}
}  // namespace

static void signal_handler(int sig) {
    (void)sig;
    g_running.store(false);
}

struct MonitorConfig {
    std::string interface = "any";
    std::string filter = "ip proto 17";  // Capture all UDP including fragments (DDS uses ephemeral ports 46000-47000)
    std::string pcap_file;               // Offline pcap file (empty = live mode)
    bool dump = false;                   // Headless offline diagnostics
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "OnDemand DDS Monitor - Real-time RTPS traffic analyzer\n\n"
              << "Options:\n"
              << "  -i, --interface <name>    Network interface (default: any)\n"
              << "  -r, --read <file>         Read from pcap file (offline mode)\n"
              << "  -f, --filter <expr>       BPF filter (default: udp)\n"
              << "  -h, --help                Show this help\n\n"
              << "Examples:\n"
              << "  sudo " << prog << " -i eth0\n"
              << "  sudo " << prog << " -i lo -f 'udp port 7410'\n"
              << "  " << prog << " -r capture.pcap\n"
              << "  " << prog << " -r capture.pcap -f 'udp port 7410'\n\n"
              << "Keyboard shortcuts:\n"
              << "  ↑/↓/j/k   Navigate participant list\n"
              << "  Enter      View participant details\n"
              << "  ESC        Back to list (from details)\n"
              << "  q          Quit\n";
}

bool parse_args(int argc, char* argv[], MonitorConfig& config) {
    static struct option long_options[] = {
        {"interface", required_argument, 0, 'i'},
        {"read", required_argument, 0, 'r'},
        {"filter", required_argument, 0, 'f'},
        {"dump", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:r:f:dh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i':
                config.interface = optarg;
                break;
            case 'r':
                config.pcap_file = optarg;
                break;
            case 'f':
                config.filter = optarg;
                break;
            case 'd':
                config.dump = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return false;
            default:
                print_usage(argv[0]);
                return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    MonitorConfig config;
    if (!parse_args(argc, argv, config)) {
        return 1;
    }

    // Check root only for live capture mode
    bool is_offline = !config.pcap_file.empty();
    if (!is_offline && geteuid() != 0) {
        std::cerr << "Error: Raw socket requires root privileges.\n"
                  << "Please run with: sudo " << argv[0] << "\n"
                  << "Or use offline mode: " << argv[0] << " -r <pcap_file>\n";
        return 1;
    }

    if (is_offline) {
        std::cout << "OnDemand Monitor (offline mode)\n"
                  << "  File: " << config.pcap_file << "\n"
                  << "  Filter: " << config.filter << "\n";
    } else {
        std::cout << "OnDemand Monitor starting...\n"
                  << "  Interface: " << config.interface << "\n"
                  << "  Filter: " << config.filter << "\n"
                  << "Press 'q' to quit.\n";
    }

    try {
        // Create components based on mode
        ondemand_monitor::PcapWorker pcap_worker = is_offline ?
            ondemand_monitor::PcapWorker(config.pcap_file) :
            ondemand_monitor::PcapWorker(config.interface, config.filter);
        ondemand_monitor::MetricsEngine metrics_engine;
        // Offline replay must age participants out in the packet-time domain,
        // not against the wall clock (see MetricsEngine::SetOfflineMode).
        metrics_engine.SetOfflineMode(is_offline);
        ondemand_monitor::MonitorUi ui(pcap_worker, metrics_engine);

        // Start capture
        pcap_worker.Start();

        if (config.dump) {
            if (!is_offline) {
                std::cerr << "--dump requires --read <pcap>\n";
                pcap_worker.Stop();
                return 1;
            }
            DrainAndDump(pcap_worker, metrics_engine);
            pcap_worker.Stop();
            return 0;
        }

        // Run UI (blocks until quit)
        ui.Run();

        // Stop capture
        pcap_worker.Stop();

        std::cout << "Monitor stopped.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
