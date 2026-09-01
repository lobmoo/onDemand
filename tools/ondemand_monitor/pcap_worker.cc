#include "pcap_worker.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

namespace ondemand_monitor {

namespace {
// memcpy-based port readers: UDP header fields are only 2-byte aligned at best
inline uint16_t ReadBe16(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof(v));
    return ntohs(v);
}
}  // namespace

PcapWorker::PcapWorker(const std::string& interface, const std::string& filter)
    : interface_(interface), filter_(filter), is_offline_(false) {
}

PcapWorker::PcapWorker(const std::string& pcap_file)
    : pcap_file_(pcap_file), is_offline_(true) {
}

PcapWorker::~PcapWorker() {
    Stop();
}

void PcapWorker::Start() {
    if (running_.load()) {
        return;
    }

    char errbuf[PCAP_ERRBUF_SIZE];

    if (is_offline_) {
        // Open pcap file
        handle_ = pcap_open_offline(pcap_file_.c_str(), errbuf);
        if (!handle_) {
            throw std::runtime_error(std::string("pcap_open_offline failed: ") + errbuf);
        }
    } else {
        // Configurable open path: unlike pcap_open_live, this lets us size the
        // kernel ring BEFORE activation so bursts absorb in the kernel instead
        // of surfacing as invisible ps_drop.
        handle_ = pcap_create(interface_.c_str(), errbuf);
        if (!handle_) {
            throw std::runtime_error(std::string("pcap_create failed: ") + errbuf);
        }
        pcap_set_snaplen(handle_, 65536);
        pcap_set_promisc(handle_, 1);
        pcap_set_timeout(handle_, 100);
        pcap_set_buffer_size(handle_, kKernelRingBytes);
        int ret = pcap_activate(handle_);
        if (ret < 0) {
            // <0: hard error; >0: warning (e.g. promisc already set) — tolerate
            std::string err = pcap_geterr(handle_);
            pcap_close(handle_);
            handle_ = nullptr;
            throw std::runtime_error("pcap_activate failed: " + err);
        }
        if (ret > 0) {
            fprintf(stderr, "pcap_activate warning: %s\n",
                    pcap_geterr(handle_));
        }
    }

    // Get link type for header parsing
    link_type_ = pcap_datalink(handle_);

    // Compile and set filter (only if filter is specified)
    if (!filter_.empty()) {
        struct bpf_program fp;
        if (pcap_compile(handle_, &fp, filter_.c_str(), 1, PCAP_NETMASK_UNKNOWN) == -1) {
            pcap_close(handle_);
            throw std::runtime_error(std::string("pcap_compile failed: ") + pcap_geterr(handle_));
        }

        if (pcap_setfilter(handle_, &fp) == -1) {
            pcap_freecode(&fp);
            pcap_close(handle_);
            throw std::runtime_error(std::string("pcap_setfilter failed: ") + pcap_geterr(handle_));
        }
        pcap_freecode(&fp);
    }

    // Start capture thread
    running_.store(true);
    finished_.store(false);
    capture_thread_ = std::thread(&PcapWorker::CaptureLoop, this);
}

void PcapWorker::Stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (handle_) {
        pcap_breakloop(handle_);
    }

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    if (handle_) {
        SampleKernelStats();
        // One-line pipeline reconciliation on exit: captured should equal
        // enqueued + kernel_dropped + malformed; anything else is a bug or an
        // in-flight backlog. Printed to stderr — the TUI is untouched.
        fprintf(stderr,
                "[monitor] capture summary: kernel_saw=%llu kernel_dropped=%llu "
                "enqueued=%llu queue_refused=%llu malformed=%llu backlog=%llu\n",
                (unsigned long long)stats_captured_.load(),
                (unsigned long long)stats_kernel_dropped_.load(),
                (unsigned long long)stats_enqueued_.load(),
                (unsigned long long)stats_enqueue_dropped_.load(),
                (unsigned long long)stats_malformed_.load(),
                (unsigned long long)queued_packets_.load());
        pcap_close(handle_);
        handle_ = nullptr;
    }
}

void PcapWorker::CaptureLoop() {
    if (is_offline_) {
        // Offline replay reads as fast as the consumer allows — throttling
        // here only stretched large captures over minutes without helping.
        struct pcap_pkthdr* header;
        const u_char* data;
        int ret;
        while ((ret = pcap_next_ex(handle_, &header, &data)) == 1) {
            if (!running_.load()) break;
            PacketHandler(reinterpret_cast<u_char*>(this), header, data);
        }
        if (ret < 0 && running_.load()) {
            fprintf(stderr, "pcap read error: %s\n", pcap_geterr(handle_));
        }
    } else {
        // dispatch loop instead of pcap_loop(-1): returns at least every
        // timeout ms, giving us a natural point to sample pcap_stats and to
        // observe running_ without relying solely on breakloop races.
        while (running_.load()) {
            int ret = pcap_dispatch(handle_, 256, PacketHandler,
                                    reinterpret_cast<u_char*>(this));
            if (ret == PCAP_ERROR) {
                fprintf(stderr, "pcap_dispatch error: %s\n", pcap_geterr(handle_));
                break;
            }
            SampleKernelStats();
        }
        SampleKernelStats();
    }

    // Mark as finished (important for offline mode)
    finished_.store(true);
}

void PcapWorker::SampleKernelStats() {
    struct pcap_stat ps;
    if (handle_ && pcap_stats(handle_, &ps) == 0) {
        stats_captured_.store(ps.ps_recv, std::memory_order_relaxed);
        stats_kernel_dropped_.store(ps.ps_drop, std::memory_order_relaxed);
    }
}

CaptureStats PcapWorker::GetCaptureStats() const {
    CaptureStats s;
    s.captured = stats_captured_.load(std::memory_order_relaxed);
    s.kernel_dropped = stats_kernel_dropped_.load(std::memory_order_relaxed);
    s.enqueued = stats_enqueued_.load(std::memory_order_relaxed);
    s.enqueue_dropped = stats_enqueue_dropped_.load(std::memory_order_relaxed);
    s.malformed = stats_malformed_.load(std::memory_order_relaxed);
    s.queue_bytes = queued_bytes_.load(std::memory_order_relaxed);
    s.queue_packets = queued_packets_.load(std::memory_order_relaxed);
    return s;
}

void PcapWorker::PacketHandler(u_char* user, const struct pcap_pkthdr* header, const u_char* packet) {
    auto* worker = reinterpret_cast<PcapWorker*>(user);

    if (!worker->running_.load()) {
        return;
    }

    // Link-layer header size per capture link type.
    // LINUX_SLL ("any" device, libpcap < 1.10): 16 bytes.
    // LINUX_SLL2 ("any" device, libpcap >= 1.10): 20 bytes — previously
    // unhandled, which misaligned every packet by 6 bytes and silently
    // discarded all traffic on newer distributions.
    uint32_t link_header_len;
    switch (worker->link_type_) {
        case DLT_LINUX_SLL:
            link_header_len = 16;
            break;
        case DLT_LINUX_SLL2:
            link_header_len = 20;
            break;
        case DLT_EN10MB:
            link_header_len = 14;
            break;
        default:
            worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
            return;
    }

    // Parse IP header
    if (header->caplen < link_header_len + 20 + 8) {
        worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
        return;  // Too short
    }

    const u_char* ip_header = packet + link_header_len;
    uint8_t ip_version = (ip_header[0] >> 4) & 0x0F;

    if (ip_version != 4) {
        worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
        return;  // Only IPv4
    }

    // Skip fragmented datagrams entirely: only the first fragment carries a
    // UDP header whose length describes the WHOLE reassembled datagram —
    // trusting it made the old code memcpy far past the fragment buffer.
    uint16_t flags_frag;
    std::memcpy(&flags_frag, ip_header + 6, sizeof(flags_frag));
    flags_frag = ntohs(flags_frag);
    if ((flags_frag & 0x3FFF) != 0) {  // MF flag or non-zero fragment offset
        worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    uint8_t ip_header_len = (ip_header[0] & 0x0F) * 4;

    if (ip_header[9] != IPPROTO_UDP) {
        worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
        return;  // Not UDP
    }

    const u_char* udp_header = ip_header + ip_header_len;
    if ((uint32_t)(udp_header - packet) + 8 > header->caplen) {
        worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
        return;  // UDP header itself truncated
    }

    // Extract UDP ports and payload
    uint16_t src_port = ReadBe16(udp_header);
    uint16_t dst_port = ReadBe16(udp_header + 2);
    uint16_t udp_len = ReadBe16(udp_header + 4);
    const u_char* udp_payload = udp_header + 8;

    if (udp_len < 8) {
        worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    uint32_t payload_len = udp_len - 8;

    // THE critical bounds check: the UDP length field can exceed what was
    // actually captured (small snaplen in offline files). Without this check
    // the memcpy below reads past the packet buffer.
    if ((uint32_t)(udp_payload - packet) + payload_len > header->caplen) {
        worker->stats_malformed_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Enqueue packet (variable-length copy)
    RawPacket pkt;
    pkt.timestamp_us = header->ts.tv_sec * 1000000ULL + header->ts.tv_usec;
    pkt.len = payload_len;
    pkt.src_port = src_port;
    pkt.dst_port = dst_port;
    // IPv4 src/dst addresses (fixed offsets in the IP header, network order)
    std::memcpy(pkt.src_ip, ip_header + 12, 4);
    std::memcpy(pkt.dst_ip, ip_header + 16, 4);
    pkt.data.resize(payload_len);
    std::memcpy(pkt.data.data(), udp_payload, payload_len);

    // Backpressure: refuse new packets once either watermark is exceeded
    // (drop-newest). Counted and visible — never silent heap growth.
    if (worker->queued_packets_.load(std::memory_order_relaxed) >= kMaxQueuedPackets ||
        worker->queued_bytes_.load(std::memory_order_relaxed) >= kMaxQueuedBytes) {
        worker->stats_enqueue_dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (!worker->queue_.enqueue(std::move(pkt))) {
        // Only fails on allocation failure inside the lock-free queue
        worker->stats_enqueue_dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    // Backlog gauges: approximate under concurrency by design (producer and
    // consumer update independently); monotonic counters above stay exact.
    worker->queued_packets_.fetch_add(1, std::memory_order_relaxed);
    worker->queued_bytes_.fetch_add(payload_len, std::memory_order_relaxed);
    worker->stats_enqueued_.fetch_add(1, std::memory_order_relaxed);
}

size_t PcapWorker::PopPackets(RawPacket* out, size_t max_count) {
    size_t n = queue_.try_dequeue_bulk(out, max_count);
    if (n > 0) {
        uint64_t bytes = 0;
        for (size_t i = 0; i < n; ++i) bytes += out[i].len;
        queued_packets_.fetch_sub(n, std::memory_order_relaxed);
        queued_bytes_.fetch_sub(bytes, std::memory_order_relaxed);
    }
    return n;
}

PcapWorker::PcapStats PcapWorker::GetPcapStats() const {
    PcapStats stats;
    if (handle_ && !is_offline_) {
        struct pcap_stat ps;
        if (pcap_stats(handle_, &ps) == 0) {
            stats.received = ps.ps_recv;
            stats.kernel_dropped = ps.ps_drop;
            stats.iface_dropped = ps.ps_ifdrop;
        }
    }
    return stats;
}

}  // namespace ondemand_monitor
