#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <arpa/inet.h>
#include <pcap.h>
#include "rtps_parser.h"

// Minimal RTPS submessage ID scanner (same logic as the inline scanner in monitor_ui.cc)
static void scan_submsg_ids(const uint8_t* data, uint32_t len, std::map<uint8_t, uint64_t>& counts) {
    if (len < 20) return;
    // Verify RTPS magic
    if (data[0] != 'R' || data[1] != 'T' || data[2] != 'P' || data[3] != 'S') return;

    const uint8_t* ptr = data + 20;
    const uint8_t* end = data + len;
    while (ptr + 4 <= end) {
        uint8_t sm_id = ptr[0];
        uint8_t flags = ptr[1];
        uint16_t otn = *reinterpret_cast<const uint16_t*>(ptr + 2);
        if (!(flags & 0x01)) otn = ntohs(otn);
        counts[sm_id]++;
        if (otn == 0) break;
        ptr += 4 + otn;
    }
}

// Parse HEARTBEAT submessage raw bytes for debugging
static void dump_heartbeat(const uint8_t* sm, uint32_t len, const uint8_t* pkt_src_prefix) {
    if (len < 32) return;
    const uint8_t* ptr = sm + 4;
    printf("    HEARTBEAT raw: writerEntityId=[%02x %02x %02x %02x] readerEntityId=[%02x %02x %02x %02x]\n",
           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
    printf("    Packet src prefix: [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
           pkt_src_prefix[0], pkt_src_prefix[1], pkt_src_prefix[2], pkt_src_prefix[3],
           pkt_src_prefix[4], pkt_src_prefix[5], pkt_src_prefix[6], pkt_src_prefix[7],
           pkt_src_prefix[8], pkt_src_prefix[9], pkt_src_prefix[10], pkt_src_prefix[11]);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <pcap_file>\n", argv[0]);
        return 1;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = pcap_open_offline(argv[1], errbuf);
    if (!handle) {
        printf("Error: %s\n", errbuf);
        return 1;
    }

    std::map<uint8_t, uint64_t> counts;
    int pkt_count = 0;
    int rtps_count = 0;

    struct pcap_pkthdr* header;
    const u_char* data;
    int ret;
    while ((ret = pcap_next_ex(handle, &header, &data)) == 1) {
        pkt_count++;

        // Parse Ethernet + IP + UDP headers to get payload
        uint32_t link_header_len = 14;  // Ethernet
        if (header->caplen < link_header_len + 20 + 8) continue;

        const uint8_t* ip_header = data + link_header_len;
        uint8_t ip_version = (ip_header[0] >> 4) & 0x0F;
        if (ip_version != 4) continue;

        uint8_t ip_header_len = (ip_header[0] & 0x0F) * 4;
        const uint8_t* udp_header = ip_header + ip_header_len;
        if (ip_header[9] != 17) continue;  // Not UDP

        uint16_t udp_len = ntohs(*reinterpret_cast<const uint16_t*>(udp_header + 4));
        const uint8_t* udp_payload = udp_header + 8;
        uint32_t payload_len = udp_len - 8;

        if (payload_len < 20) continue;
        if (udp_payload[0] != 'R' || udp_payload[1] != 'T' ||
            udp_payload[2] != 'P' || udp_payload[3] != 'S') continue;

        rtps_count++;
        scan_submsg_ids(udp_payload, payload_len, counts);

        // If this packet has HEARTBEAT (0x07), dump details
        bool has_hb = false;
        const uint8_t* ptr = udp_payload + 20;
        const uint8_t* end = udp_payload + payload_len;
        while (ptr + 4 <= end) {
            if (ptr[0] == 0x07) { has_hb = true; break; }
            uint8_t flags = ptr[1];
            uint16_t otn = *reinterpret_cast<const uint16_t*>(ptr + 2);
            if (!(flags & 0x01)) otn = ntohs(otn);
            if (otn == 0) break;
            ptr += 4 + otn;
        }
        if (has_hb) {
            // Walk again to dump HB details
            ptr = udp_payload + 20;
            while (ptr + 4 <= end) {
                uint8_t sm_id = ptr[0];
                uint8_t flags = ptr[1];
                uint16_t otn = *reinterpret_cast<const uint16_t*>(ptr + 2);
                if (!(flags & 0x01)) otn = ntohs(otn);
                if (sm_id == 0x07) {
                    dump_heartbeat(ptr, (otn == 0) ? (uint32_t)(end - ptr) : otn + 4, udp_payload + 8);
                }
                if (otn == 0) break;
                ptr += 4 + otn;
            }
        }
    }

    pcap_close(handle);

    printf("\n=== Results ===\n");
    printf("Total packets: %d\n", pkt_count);
    printf("RTPS packets: %d\n", rtps_count);
    printf("\nSubmessage ID distribution:\n");
    for (auto& [id, count] : counts) {
        const char* name = "???";
        switch (id) {
            case 0x15: name = "DATA"; break;
            case 0x16: name = "DATA_FRAG"; break;
            case 0x07: name = "HEARTBEAT"; break;
            case 0x06: name = "ACKNACK"; break;
            case 0x08: name = "GAP"; break;
            case 0x09: name = "INFO_TS"; break;
            case 0x0e: name = "INFO_DST"; break;
            case 0x0c: name = "INFO_SRC"; break;
            case 0x12: name = "NACK_FRAG"; break;
            case 0x13: name = "HEARTBEAT_FRAG"; break;
        }
        printf("  0x%02x %-15s %llu\n", id, name, (unsigned long long)count);
    }

    return 0;
}
