#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <pcap.h>
#include "concurrentqueue.h"

namespace ondemand_monitor {

struct RawPacket {
    uint64_t timestamp_us;
    uint32_t len;
    uint16_t src_port = 0;   // UDP source port
    uint16_t dst_port = 0;   // UDP destination port
    uint8_t data[65536];  // Max UDP payload
};

class PcapWorker {
public:
    PcapWorker(const std::string& interface, const std::string& filter);
    ~PcapWorker();

    // Non-copyable
    PcapWorker(const PcapWorker&) = delete;
    PcapWorker& operator=(const PcapWorker&) = delete;

    void Start();
    void Stop();

    // Consumer interface
    size_t PopPackets(RawPacket* out, size_t max_count);
    uint64_t GetDropped() const { return dropped_.load(); }

private:
    void CaptureLoop();
    static void PacketHandler(u_char* user, const struct pcap_pkthdr* header, const u_char* packet);

    std::string interface_;
    std::string filter_;
    pcap_t* handle_ = nullptr;
    int link_type_ = 0;  // DLT_EN10MB=1, DLT_LINUX_SLL=113

    std::atomic<bool> running_{false};
    std::thread capture_thread_;

    moodycamel::ConcurrentQueue<RawPacket> queue_;
    std::atomic<uint64_t> dropped_{0};
};

}  // namespace ondemand_monitor
