#pragma once

#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCAP_ERRBUF_SIZE 256
#define PCAP_NETMASK_UNKNOWN 0xffffffff
#define PCAP_ERROR -1

// Link types
#define DLT_EN10MB 1
#define DLT_LINUX_SLL 113

// pcap handle (opaque)
typedef struct pcap pcap_t;

// BPF program
struct bpf_program {
    uint32_t bf_len;
    void *bf_insns;
};

// Packet header
struct pcap_pkthdr {
    struct timeval ts;    // timestamp
    uint32_t caplen;      // captured length
    uint32_t len;         // original length
};

// Callback type
typedef void (*pcap_handler)(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes);

// Functions
pcap_t *pcap_open_live(const char *device, int snaplen, int promisc, int to_ms, char *errbuf);
int pcap_compile(pcap_t *p, struct bpf_program *fp, const char *str, int optimize, uint32_t netmask);
int pcap_setfilter(pcap_t *p, struct bpf_program *fp);
int pcap_datalink(pcap_t *p);
int pcap_loop(pcap_t *p, int cnt, pcap_handler callback, u_char *user);
void pcap_breakloop(pcap_t *p);
void pcap_close(pcap_t *p);
void pcap_freecode(struct bpf_program *fp);
const char *pcap_geterr(pcap_t *p);

#ifdef __cplusplus
}
#endif
