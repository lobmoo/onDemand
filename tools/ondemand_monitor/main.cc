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
    std::string filter = "udp";  // Capture all UDP traffic to find SEDP packets
};

void print_usage(const char* prog) {
    std::cout << "Usage: sudo " << prog << " [OPTIONS]\n"
              << "OnDemand DDS Monitor - Real-time RTPS traffic analyzer\n\n"
              << "Options:\n"
              << "  -i, --interface <name>    Network interface (default: any)\n"
              << "  -f, --filter <expr>       BPF filter (default: udp port 7410 or udp port 7411)\n"
              << "  -h, --help                Show this help\n\n"
              << "Examples:\n"
              << "  sudo " << prog << " -i eth0\n"
              << "  sudo " << prog << " -i lo -f 'udp port 7410'\n"
              << "  sudo " << prog << " -i any -f 'udp portrange 7400-7500'\n\n"
              << "Keyboard shortcuts:\n"
              << "  ↑/↓/j/k   Navigate participant list\n"
              << "  Enter      View participant details\n"
              << "  ESC        Back to list (from details)\n"
              << "  q          Quit\n";
}

bool parse_args(int argc, char* argv[], MonitorConfig& config) {
    static struct option long_options[] = {
        {"interface", required_argument, 0, 'i'},
        {"filter", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:f:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i':
                config.interface = optarg;
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

    // Check root
    if (geteuid() != 0) {
        std::cerr << "Error: Raw socket requires root privileges.\n"
                  << "Please run with: sudo " << argv[0] << "\n";
        return 1;
    }

    std::cout << "OnDemand Monitor starting...\n"
              << "  Interface: " << config.interface << "\n"
              << "  Filter: " << config.filter << "\n"
              << "Press 'q' to quit.\n";

    try {
        // Create components
        ondemand_monitor::PcapWorker pcap_worker(config.interface, config.filter);
        ondemand_monitor::MetricsEngine metrics_engine;
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
