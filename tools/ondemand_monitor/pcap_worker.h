#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <pcap.h>
#include "concurrentqueue.h"

namespace ondemand_monitor {

// Wire packet handed from the capture thread to the processing thread.
// The payload is stored variable-length: the old inline data[65536] made every
// queued element ~65.5KB regardless of the actual size (typical RTPS packets
// are <=1500B, a ~40x memory blowup) and turned any consumer backlog into an
// OOM risk instead of a measurable queue depth.
struct RawPacket {
    uint64_t timestamp_us = 0;
    uint32_t len = 0;         // UDP payload length (== data.size())
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    uint8_t src_ip[4] = {0};  // IPv4 source address (network byte order)
    uint8_t dst_ip[4] = {0};  // IPv4 destination address (network byte order)
    std::vector<uint8_t> data;  // exact-size UDP payload
};

// Per-layer capture counters. Together they let every stage of the pipeline be
// reconciled: captured(BPF-accepted) == enqueued + kernel_dropped + malformed,
// and enqueued drains into consumers via PopPackets. A gap in any window pins
// the loss to that specific layer.
struct CaptureStats {
    uint64_t captured = 0;         // BPF-accepted packets seen by pcap_stats
    uint64_t kernel_dropped = 0;   // dropped because the kernel ring was full
    uint64_t enqueued = 0;         // handed to the inter-thread queue
    uint64_t enqueue_dropped = 0;  // refused: bounded-queue watermarks exceeded
    uint64_t malformed = 0;        // truncated / non-IPv4 / fragmented / short
    uint64_t queue_bytes = 0;      // current backlog (approximate gauges)
    uint64_t queue_packets = 0;
};

class PcapWorker {
public:
    // Live capture mode
    PcapWorker(const std::string& interface, const std::string& filter);
    // Offline mode (read from pcap file)
    PcapWorker(const std::string& pcap_file);
    ~PcapWorker();

    // Non-copyable
    PcapWorker(const PcapWorker&) = delete;
    PcapWorker& operator=(const PcapWorker&) = delete;

    void Start();
    void Stop();

    // Consumer interface
    size_t PopPackets(RawPacket* out, size_t max_count);

    // UI-facing drop counter, unchanged semantics: packets refused at enqueue.
    uint64_t GetDropped() const {
        return stats_enqueue_dropped_.load(std::memory_order_relaxed);
    }
    CaptureStats GetCaptureStats() const;
    bool IsOffline() const { return is_offline_; }
    bool IsFinished() const { return finished_.load(); }

    // Kernel-level pcap stats (packets received, dropped by kernel, dropped by iface)
    struct PcapStats {
        uint32_t received = 0;
        uint32_t kernel_dropped = 0;   // ps_drop: kernel buffer overflow
        uint32_t iface_dropped = 0;    // ps_ifdrop: NIC ring buffer overflow
    };
    PcapStats GetPcapStats() const;

private:
    void CaptureLoop();
    // Refresh captured/kernel_dropped from libpcap (live mode only)
    void SampleKernelStats();
    static void PacketHandler(u_char* user, const struct pcap_pkthdr* header, const u_char* packet);

    std::string interface_;
    std::string filter_;
    std::string pcap_file_;
    bool is_offline_ = false;
    pcap_t* handle_ = nullptr;
    int link_type_ = 0;

    std::atomic<bool> running_{false};
    std::atomic<bool> finished_{false};  // True when offline file is fully read
    std::thread capture_thread_;

    moodycamel::ConcurrentQueue<RawPacket> queue_;

    // Layered counters (relaxed ordering suffices: independent monotonic
    // gauges consumed for display/reconciliation, not for synchronization)
    std::atomic<uint64_t> stats_captured_{0};
    std::atomic<uint64_t> stats_kernel_dropped_{0};
    std::atomic<uint64_t> stats_enqueued_{0};
    std::atomic<uint64_t> stats_enqueue_dropped_{0};
    std::atomic<uint64_t> stats_malformed_{0};
    std::atomic<uint64_t> queued_packets_{0};
    std::atomic<uint64_t> queued_bytes_{0};

    // Backpressure watermarks. Byte budget is the binding one (~256MB); the
    // packet cap only guards against pathological all-jumbo backlogs. With a
    // healthy consumer these never fire — their role is to turn "consumer too
    // slow" from unbounded heap growth into a counted, visible loss.
    static constexpr uint64_t kMaxQueuedBytes = 256ull << 20;
    static constexpr uint64_t kMaxQueuedPackets = 262144;
    // Kernel ring buffer requested at activation (128MB): sized so that burst
    // absorption happens in the kernel instead of becoming ps_drop.
    static constexpr int kKernelRingBytes = 128 << 20;
};

}  // namespace ondemand_monitor
