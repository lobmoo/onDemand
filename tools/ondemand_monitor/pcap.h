#pragma once

#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Basic types
typedef unsigned char u_char;

#define PCAP_ERRBUF_SIZE 256
#define PCAP_NETMASK_UNKNOWN 0xffffffff
#define PCAP_ERROR -1

// Link types
#define DLT_EN10MB 1
#define DLT_LINUX_SLL 113
#define DLT_LINUX_SLL2 276  // libpcap >= 1.10 default for "-i any"

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

// Interface statistics (layout matches libpcap's own definition: three u_int)
struct pcap_stat {
    uint32_t ps_recv;    // packets that passed the filter into the kernel ring
    uint32_t ps_drop;    // packets dropped by the kernel ring (buffer full)
    uint32_t ps_ifdrop;  // dropped by the network interface
};

// Callback type
typedef void (*pcap_handler)(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes);

// Functions
pcap_t *pcap_open_live(const char *device, int snaplen, int promisc, int to_ms, char *errbuf);
pcap_t *pcap_open_offline(const char *fname, char *errbuf);

// Configurable open path (libpcap >= 1.0): allows setting the kernel ring
// buffer size BEFORE activation — pcap_open_live cannot do this, which is why
// bursts used to silently overflow the ~2MB default ring.
pcap_t *pcap_create(const char *source, char *errbuf);
int pcap_set_snaplen(pcap_t *p, int snaplen);
int pcap_set_promisc(pcap_t *p, int promisc);
int pcap_set_timeout(pcap_t *p, int to_ms);
int pcap_set_buffer_size(pcap_t *p, int buffer_size);
int pcap_activate(pcap_t *p);
int pcap_stats(pcap_t *p, struct pcap_stat *ps);
int pcap_compile(pcap_t *p, struct bpf_program *fp, const char *str, int optimize, uint32_t netmask);
int pcap_setfilter(pcap_t *p, struct bpf_program *fp);
int pcap_datalink(pcap_t *p);
int pcap_loop(pcap_t *p, int cnt, pcap_handler callback, u_char *user);
int pcap_dispatch(pcap_t *p, int cnt, pcap_handler callback, u_char *user);
int pcap_next_ex(pcap_t *p, struct pcap_pkthdr **pkt_header, const u_char **pkt_data);
const u_char *pcap_next(pcap_t *p, struct pcap_pkthdr *h);
void pcap_breakloop(pcap_t *p);
void pcap_close(pcap_t *p);
void pcap_freecode(struct bpf_program *fp);
const char *pcap_geterr(pcap_t *p);

#ifdef __cplusplus
}
#endif
