#include "pcap_worker.h"
#include <iostream>
#include <cstring>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

namespace ondemand_monitor {

PcapWorker::PcapWorker(const std::string& interface, const std::string& filter)
    : interface_(interface), filter_(filter) {
}

PcapWorker::~PcapWorker() {
    Stop();
}

void PcapWorker::Start() {
    if (running_.load()) {
        return;
    }

    char errbuf[PCAP_ERRBUF_SIZE];

    // Open device
    handle_ = pcap_open_live(interface_.c_str(), 65536, 1, 100, errbuf);
    if (!handle_) {
        throw std::runtime_error(std::string("pcap_open_live failed: ") + errbuf);
    }

    // Get link type for header parsing
    link_type_ = pcap_datalink(handle_);

    // Compile filter
    struct bpf_program fp;
    if (pcap_compile(handle_, &fp, filter_.c_str(), 1, PCAP_NETMASK_UNKNOWN) == -1) {
        pcap_close(handle_);
        throw std::runtime_error(std::string("pcap_compile failed: ") + pcap_geterr(handle_));
    }

    // Set filter
    if (pcap_setfilter(handle_, &fp) == -1) {
        pcap_freecode(&fp);
        pcap_close(handle_);
        throw std::runtime_error(std::string("pcap_setfilter failed: ") + pcap_geterr(handle_));
    }
    pcap_freecode(&fp);

    // Start capture thread
    running_.store(true);
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
        pcap_close(handle_);
        handle_ = nullptr;
    }
}

void PcapWorker::CaptureLoop() {
    int ret = pcap_loop(handle_, -1, PacketHandler, reinterpret_cast<u_char*>(this));
    if (ret == PCAP_ERROR) {
        fprintf(stderr, "pcap_loop error: %s\n", pcap_geterr(handle_));
    }
}

void PcapWorker::PacketHandler(u_char* user, const struct pcap_pkthdr* header, const u_char* packet) {
    auto* worker = reinterpret_cast<PcapWorker*>(user);

    if (!worker->running_.load()) {
        return;
    }

    // Determine link layer header size based on pcap link type
    // LINUX_SLL header format:
    //   2 bytes: packet type
    //   2 bytes: ARPHRD type
    //   2 bytes: link-layer address length
    //   8 bytes: link-layer address
    //   2 bytes: protocol type (IPv4 = 0x0800)
    // Total: 16 bytes
    uint32_t link_header_len = 14;  // Default to Ethernet (DLT_EN10MB)
    if (worker->link_type_ == 113) {  // DLT_LINUX_SLL
        link_header_len = 16;
    }

    // Parse IP header
    if (header->caplen < link_header_len + 20 + 8) {
        return;  // Too short
    }

    const u_char* ip_header = packet + link_header_len;
    uint8_t ip_version = (ip_header[0] >> 4) & 0x0F;

    if (ip_version != 4) {
        return;  // Only IPv4
    }

    uint8_t ip_header_len = (ip_header[0] & 0x0F) * 4;
    const u_char* udp_header = ip_header + ip_header_len;

    if (ip_header[9] != IPPROTO_UDP) {
        return;  // Not UDP
    }

    // Extract UDP ports and payload
    uint16_t src_port = ntohs(*reinterpret_cast<const uint16_t*>(udp_header));
    uint16_t dst_port = ntohs(*reinterpret_cast<const uint16_t*>(udp_header + 2));
    uint16_t udp_len = ntohs(*reinterpret_cast<const uint16_t*>(udp_header + 4));
    const u_char* udp_payload = udp_header + 8;
    uint32_t payload_len = udp_len - 8;

    if (payload_len == 0 || payload_len > 65536) {
        return;
    }

    // Enqueue packet
    RawPacket pkt;
    pkt.timestamp_us = header->ts.tv_sec * 1000000ULL + header->ts.tv_usec;
    pkt.len = payload_len;
    pkt.src_port = src_port;
    pkt.dst_port = dst_port;
    std::memcpy(pkt.data, udp_payload, payload_len);

    if (!worker->queue_.enqueue(pkt)) {
        worker->dropped_.fetch_add(1);
    }
}

size_t PcapWorker::PopPackets(RawPacket* out, size_t max_count) {
    return queue_.try_dequeue_bulk(out, max_count);
}

}  // namespace ondemand_monitor
