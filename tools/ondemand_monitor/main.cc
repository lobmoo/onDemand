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

namespace {
std::atomic<bool> g_running{true};
}

static void signal_handler(int sig) {
    (void)sig;
    g_running.store(false);
}

struct MonitorConfig {
    std::string interface = "any";
    std::string filter = "udp portrange 7400-7500";  // Capture DDS traffic (SPDP/SEDP/data)
    std::string pcap_file;       // Offline pcap file (empty = live mode)
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
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:r:f:h", long_options, nullptr)) != -1) {
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
