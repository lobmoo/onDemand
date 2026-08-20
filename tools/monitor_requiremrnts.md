# OnDemand DDS Monitor v4.0 — 需求规格文档

> 版本: v4.0
> 日期: 2026-08-19
> 状态: 待开发
> 预计工期: 5-7天

---

## 1. 项目概述

### 1.1 目标

在现有 monitor v3.0 基础上，增强 RTPS 协议解析能力，实现：
1. **PDP 报文深度解析** — 自动发现所有 DDS 参与者（Participant）
2. **Topic 匹配分析** — 可视化展示 Publisher/Subscriber 匹配状态
3. **传输质量统计** — 基于 SN/ACK/NACK 的丢包率、重传率统计
4. **JSON 数据导出** — 后台定时生成 JSON 快照，供 Web 前端消费

### 1.2 架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                        Monitor v4.0                              │
├─────────────────────────────────────────────────────────────────┤
│  Layer 1: Raw Socket 抓包                                        │
│  ├── PDP 报文解析 (SPDP)                                         │
│  │   ├── Domain ID                                              │
│  │   ├── Participant GUID                                       │
│  │   ├── Participant Name                                       │
│  │   ├── Metatraffic Locator (unicast/multicast)                │
│  │   └── Lease Duration (超时检测)                               │
│  │                                                              │
│  ├── SEDP 报文解析                                               │
│  │   ├── Publisher/Subscriber Endpoint GUID                     │
│  │   ├── Topic Name                                             │
│  │   ├── Type Name                                              │
│  │   ├── QoS (reliability, durability 等)                       │
│  │   └── Partition                                              │
│  │                                                              │
│  └── 数据传输统计                                                │
│      ├── SN (Sequence Number) 追踪                              │
│      ├── ACK/NACK 解析                                          │
│      ├── Heartbeat 追踪                                         │
│      └── Fragment 统计 (DATA_FRAG)                              │
│                                                                 │
│  Layer 2: Fast DDS 订阅 (保留)                                   │
│  └── PubTableDefine / SubTableRegister                          │
│                                                                 │
│  输出层:                                                         │
│  ├── TUI (FTXUI) — 终端实时显示                                  │
│  └── JSON Exporter — 定时输出到文件/Socket                       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 页面 0 — PDP Discovery (参与者发现)

### 2.1 功能描述

扫描环网中所有 RTPS 报文，从 SPDP (Simple Participant Discovery Protocol) 报文中解析出每个 DDS 参与者的完整信息，并实时显示。

### 2.2 数据模型

```cpp
// 新增数据结构 — ParticipantInfo
struct ParticipantInfo {
    // === 从 PDP 报文解析 ===
    std::string guid_prefix;          // 12字节 GUID prefix (hex string, 如 "010f0078 61040000")
    uint32_t domain_id;               // Domain ID (从 PID_DOMAIN_ID 解析)
    std::string participant_name;     // 从 PID_PARTICIPANT_NAME 解析
    uint32_t participant_key;         // ParticipantKey (from PID_PARTICIPANT_KEY)

    // 网络定位信息
    struct Locator {
        uint32_t kind;      // 1=UDPv4, 2=UDPv6, 16=SHM
        uint32_t port;
        std::string address; // IP 地址
    };
    std::vector<Locator> metatraffic_unicast;    // 单播定位器
    std::vector<Locator> metatraffic_multicast;  // 组播定位器
    std::vector<Locator> default_unicast;        // 默认单播定位器
    std::vector<Locator> default_multicast;      // 默认组播定位器

    // === 状态信息 ===
    double first_seen;                // 首次发现时间 (steady_clock)
    double last_seen;                 // 最后一次 PDP 时间
    int pdp_count;                    // PDP 报文计数
    bool is_alive;                    // 是否活跃 (5秒内有PDP)

    // === 关联信息 ===
    int packet_count;                 // 该参与者发送的总包数
    std::set<std::string> endpoints;  // 关联的 Endpoint GUID 列表
    std::set<std::string> topics;     // 关联的 Topic 列表

    // === 构造函数 ===
    ParticipantInfo() : domain_id(0), participant_key(0),
                       first_seen(0), last_seen(0), pdp_count(0),
                       is_alive(false), packet_count(0) {}
};

// PDP 报文详情 (用于详情面板)
struct PdpPacketDetail {
    double timestamp;
    std::string src_ip;
    uint16_t src_port;
    uint16_t dst_port;
    std::string guid_prefix;
    uint32_t domain_id;
    std::string participant_name;
    int packet_size;
    // 解析出的所有 PID 列表 (调试用)
    std::vector<std::pair<uint16_t, std::string>> params;
};
```

### 2.3 TUI 界面设计

```
┌─ [0] Discovery ─ [1] Topics ─ [2] Match ─ [3] Variables ─ ────┐
│ PDP Participant Scanner                                         │
│                                                                 │
│ ┌─ Participants ──────────────────────────────────────────────┐ │
│ │ Domain │ GUID Prefix    │ Name              │ Ports  │Status│ │
│ ├────────┼────────────────┼───────────────────┼────────┼──────┤ │
│ │ 77     │ 010f007861...  │ pubNode           │ 36140  │ ● ON │ │
│ │ 77     │ 010f007861...  │ subNode41975      │ 59958  │ ● ON │ │
│ │ 77     │ 010f007861...  │ monitor           │ 45678  │ ○OFF │ │
│ └────────┴────────────────┴───────────────────┴────────┴──────┘ │
│                                                                 │
│ ┌─ Participant Detail (selected) ─────────────────────────────┐ │
│ │ GUID: 010f0078610400000000000000000000                       │ │
│ │ Name: pubNode                                                │ │
│ │ Domain: 77                                                   │ │
│ │ First Seen: 2026-08-19 14:30:01                             │ │
│ │ Last Seen:  2026-08-19 14:35:22                             │ │
│ │ PDP Count: 120                                               │ │
│ │ Metatraffic Unicast: 192.168.1.100:36140                    │ │
│ │ Metatraffic Multicast: 239.255.0.1:7400                     │ │
│ │ Endpoints: 5 (3 pub, 2 sub)                                 │ │
│ │ Topics: bucket_0, bucket_1, bucket_2, ...                   │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ┌─ Recent PDP Packets ────────────────────────────────────────┐ │
│ │ Time     │ Src→Dst      │ Domain │ GUID Prefix    │ Name   │ │
│ ├──────────┼──────────────┼────────┼────────────────┼────────┤ │
│ │ 14:35:22 │ 36140→7400   │ 77     │ 010f007861...  │ pubN.. │ │
│ │ 14:35:21 │ 59958→7400   │ 77     │ 010f007861...  │ subN.. │ │
│ └──────────┴──────────────┴────────┴────────────────┴────────┘ │
│                                                                 │
│ Stats: 3 Participants │ 360 PDP pkts │ 2 Active │ 1 Inactive   │
│ [Tab] Switch Panel  [↑↓] Select  [Enter] Detail  [Q] Quit      │
└─────────────────────────────────────────────────────────────────┘
```

### 2.4 RTPS 解析要求

#### 2.4.1 SPDP 报文结构

SPDP 报文是 DATA 子消息，Writer Entity ID = `0x000100c2` (ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)

需要解析的 Parameter ID (PID):

| PID | 值 | 类型 | 说明 |
|-----|-----|------|------|
| PID_DOMAIN_ID | 0x000F | uint32_t | Domain ID |
| PID_PARTICIPANT_GUID | 0x0050 | GUID_t (16 bytes) | Participant GUID |
| PID_PARTICIPANT_KEY | 0x005A | GUID_t (16 bytes) | Participant Key |
| PID_PARTICIPANT_NAME | 0x0060 | string | Participant 名称 |
| PID_METATRAFFIC_UNICAST_LOCATOR | 0x0032 | Locator (24 bytes) | 元数据单播定位器 |
| PID_METATRAFFIC_MULTICAST_LOCATOR | 0x0033 | Locator (24 bytes) | 元数据组播定位器 |
| PID_DEFAULT_UNICAST_LOCATOR | 0x0031 | Locator (24 bytes) | 默认单播定位器 |
| PID_DEFAULT_MULTICAST_LOCATOR | 0x0040 | Locator (24 bytes) | 默认组播定位器 |
| PID_LEASE_DURATION | 0x0002 | Duration_t (8 bytes) | 租约时长 |

#### 2.4.2 Locator 结构

```cpp
struct RtpsLocator {
    uint32_t kind;       // 0=INVALID, 1=UDPv4, 2=UDPv6, 16=SHM
    uint32_t port;       // 端口号
    uint8_t address[16]; // 地址 (IPv4 用前4字节)
};
```

#### 2.4.3 解析代码扩展

在 `rtps_parser.hpp/cpp` 中新增:

```cpp
namespace rtps {

// 新增 PID 定义
enum QosPidExtended : uint16_t {
    PID_PARTICIPANT_NAME        = 0x0060,
    PID_METATRAFFIC_UNICAST     = 0x0032,
    PID_METATRAFFIC_MULTICAST   = 0x0033,
    PID_DEFAULT_UNICAST         = 0x0031,
    PID_DEFAULT_MULTICAST       = 0x0040,
    PID_LEASE_DURATION          = 0x0002,
    PID_PARTICIPANT_KEY         = 0x0050,
};

// Locator 解析
struct Locator {
    uint32_t kind;
    uint32_t port;
    std::string address;  // "192.168.1.100" 格式
};

// PDP 完整解析结果
struct PdpParseResult {
    bool valid = false;
    uint32_t domain_id = 0;
    std::string guid_prefix;      // 24字符 hex
    std::string participant_name;
    std::vector<Locator> metatraffic_unicast;
    std::vector<Locator> metatraffic_multicast;
    std::vector<Locator> default_unicast;
    std::vector<Locator> default_multicast;
    uint32_t lease_duration_sec = 0;
};

// 解析 PDP 报文
PdpParseResult parse_spdp_full(const uint8_t* payload, int submsg_offset,
                                int submsg_len, bool is_be);

// 解析 Locator
Locator parse_locator(const uint8_t* buf, bool is_be);

// 格式化 GUID prefix
std::string format_guid_prefix(const uint8_t* guid_bytes);

} // namespace rtps
```

### 2.5 MetricsEngine 扩展

```cpp
class MetricsEngine {
    // ... 现有成员 ...

    // 新增: Participant 追踪
    std::map<std::string, ParticipantInfo> participants_;  // guid_prefix -> info

    // 新增回调
    void on_pdp_packet(const rtps::PdpParseResult& pdp, int src_port, int dst_port, int pkt_size);

    // 新增: 超时检测 (5秒)
    void check_participant_timeout();

    // 快照扩展
    Snapshot snapshot() const;  // 包含 participants 列表
};

// Snapshot 扩展
struct Snapshot {
    // ... 现有字段 ...
    std::vector<ParticipantInfo> participants;  // 新增
    int active_participants = 0;                // 新增
    int inactive_participants = 0;              // 新增
};
```

### 2.6 超时检测逻辑

```cpp
void MetricsEngine::check_participant_timeout() {
    double now = wall_now();
    constexpr double TIMEOUT_SEC = 5.0;

    for (auto& [guid, info] : participants_) {
        if (info.is_alive && (now - info.last_seen) > TIMEOUT_SEC) {
            info.is_alive = false;
            events_.push_back("[PDP] Participant INACTIVE: " + info.participant_name +
                            " (" + guid.substr(0, 12) + ")");
        }
    }
}

void MetricsEngine::on_pdp_packet(const rtps::PdpParseResult& pdp,
                                   int src_port, int dst_port, int pkt_size) {
    std::lock_guard<std::mutex> lk(mtx_);

    auto& p = participants_[pdp.guid_prefix];
    p.guid_prefix = pdp.guid_prefix;
    p.domain_id = pdp.domain_id;
    p.participant_name = pdp.participant_name;
    p.metatraffic_unicast = pdp.metatraffic_unicast;
    p.metatraffic_multicast = pdp.metatraffic_multicast;
    p.default_unicast = pdp.default_unicast;
    p.default_multicast = pdp.default_multicast;

    double now = wall_now();
    if (!p.first_seen) p.first_seen = now;
    p.last_seen = now;
    p.pdp_count++;
    p.is_alive = true;
    p.packet_count++;

    // 记录端口
    // ...

    // 事件日志
    if (p.pdp_count == 1) {
        events_.push_back("[PDP] New participant: " + pdp.participant_name +
                        " domain=" + std::to_string(pdp.domain_id));
    }
}
```

### 2.7 PcapWorker 修改

```cpp
void PcapWorker::process(const uint8_t* data, int len) {
    // ... 现有解析 ...

    for (auto& sm : res.submsgs) {
        // ... 现有逻辑 ...

        // PDP 报文处理 (增强版)
        if (sm.writer_id == rtps::ENTITYID_SPDP_WRITER) {
            // 原有: 只解析 domain_id
            // int did = rtps::parse_spdp_domain_id(...);

            // 新增: 完整解析 PDP
            auto pdp = rtps::parse_spdp_full(pl, sm.offset, sm.length, sm.is_be);
            if (pdp.valid) {
                parsed_domain_id_ = pdp.domain_id;
                metrics_.on_pdp_packet(pdp, sp, dp, pz);
                metrics_.on_domain_discovered(pdp.domain_id, pdp.guid_prefix, sp, dp, pz);
            }
        }

        // ... 其余逻辑 ...
    }
}
```

---

## 3. 页面 1 — Topic 匹配分析

### 3.1 功能描述

从 SEDP (Simple Endpoint Discovery Protocol) 报文中解析出所有 Publisher 和 Subscriber 的 Endpoint 信息，分析它们之间的 Topic 匹配关系。

### 3.2 数据模型

```cpp
// Endpoint 信息 (从 SEDP 解析)
struct EndpointInfo {
    std::string endpoint_guid;        // 完整 GUID (hex)
    std::string participant_guid;     // 所属 Participant GUID prefix
    std::string topic_name;
    std::string type_name;
    bool is_publisher;                // true=Writer, false=Reader

    // QoS 信息
    struct QosInfo {
        int reliability;              // 0=BEST_EFFORT, 1=RELIABLE
        int durability;               // 0=VOLATILE, 1=TRANSIENT_LOCAL, 2=TRANSIENT, 3=PERSISTENT
        int ownership;                // 0=SHARED, 1=EXCLUSIVE
        int liveliness;               // 0=AUTOMATIC, 1=MANUAL_BY_PARTICIPANT, 2=MANUAL_BY_TOPIC
        int deadline_sec;             // Deadline (秒)
        int history_depth;            // History depth
    } qos;

    // 统计信息
    double first_seen;
    double last_seen;
    bool is_alive;                    // 5秒内有 SEDP 或数据
    int data_packet_count;            // 数据包计数
};

// Topic 匹配信息
struct TopicMatch {
    std::string topic_name;
    std::string type_name;

    // 匹配的端点
    struct MatchedPair {
        std::string pub_guid;         // Publisher endpoint GUID
        std::string pub_participant;  // Publisher participant name
        std::string sub_guid;         // Subscriber endpoint GUID
        std::string sub_participant;  // Subscriber participant name
        bool qos_compatible;          // QoS 是否兼容
        std::string mismatch_reason;  // 不兼容原因
    };
    std::vector<MatchedPair> matched_pairs;

    // 未匹配的端点
    struct UnmatchedEndpoint {
        std::string guid;
        std::string participant;
        bool is_publisher;
        std::string reason;           // 未匹配原因
    };
    std::vector<UnmatchedEndpoint> unmatched_endpoints;

    // 统计
    int pub_count = 0;
    int sub_count = 0;
    int matched_count = 0;
};

// Topic 传输统计
struct TopicTransferStats {
    std::string topic_name;
    std::string writer_guid;          // Writer endpoint GUID

    // 包统计
    uint64_t total_packets = 0;       // 总包数
    uint64_t complete_packets = 0;    // 完整包数 (非分片)
    uint64_t fragmented_packets = 0;  // 分片包数
    uint64_t total_fragments = 0;     // 总分片数

    // SN 统计
    uint64_t sn_expected = 0;         // 期望 SN
    uint64_t sn_received = 0;         // 实际接收 SN
    uint64_t sn_lost = 0;             // 丢失 SN 数
    uint64_t sn_duplicate = 0;        // 重复 SN 数

    // ACK/NACK 统计
    uint64_t ack_count = 0;           // ACK 数量
    uint64_t nack_count = 0;          // NACK 数量
    uint64_t heartbeat_count = 0;     // Heartbeat 数量
    uint64_t gap_count = 0;           // GAP 数量

    // 计算指标
    double loss_rate() const {
        uint64_t total = sn_received + sn_lost;
        return total > 0 ? (double)sn_lost / total : 0.0;
    }

    double retransmission_rate() const {
        // 基于 NACK/ACK 比率估算
        uint64_t total_feedback = ack_count + nack_count;
        return total_feedback > 0 ? (double)nack_count / total_feedback : 0.0;
    }

    double frag_ratio() const {
        return total_packets > 0 ? (double)fragmented_packets / total_packets : 0.0;
    }

    // 速率
    double packets_per_sec = 0;
    double bytes_per_sec = 0;
    uint64_t total_bytes = 0;
};
```

### 3.3 TUI 界面设计

```
┌─ [0] Discovery ─ [1] Topics ─ [2] Match ─ [3] Variables ─ ────┐
│ Topic Match Analysis                                            │
│                                                                 │
│ ┌─ Topic Match Matrix ────────────────────────────────────────┐ │
│ │ Topic                    │ Pub    │ Sub    │ Match │ Status │ │
│ ├──────────────────────────┼────────┼────────┼───────┼────────┤ │
│ │ dsf/var/data/transfer/b0 │ pubN.. │ subN.. │ 2/3   │ ⚠ WARN │ │
│ │ dsf/var/data/transfer/b1 │ pubN.. │ subN.. │ 3/3   │ ✓ OK   │ │
│ │ dsf/sys/var/tableDefine  │ pubN.. │ mon..  │ 1/1   │ ✓ OK   │ │
│ │ dsf/message/subTable..   │ pubN.. │ mon..  │ 1/2   │ ✗ FAIL │ │
│ └──────────────────────────┴────────┴────────┴───────┴────────┘ │
│                                                                 │
│ ┌─ Match Detail: dsf/var/data/transfer/bucket_0 ──────────────┐ │
│ │ Matched Pairs:                                               │ │
│ │   pubNode:03030000 → subNode41975:03040000  [QoS: ✓]        │ │
│ │   pubNode:03030000 → subNode2:03050000      [QoS: ✓]        │ │
│ │                                                              │ │
│ │ Unmatched:                                                   │ │
│ │   subNode3:03060000 (Reader) — No matching writer found      │ │
│ │                                                              │ │
│ │ QoS Compatibility:                                           │ │
│ │   Reliability: RELIABLE ↔ RELIABLE ✓                         │ │
│ │   Durability: VOLATILE ↔ VOLATILE ✓                          │ │
│ └──────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ [Tab] Switch Panel  [↑↓] Select  [Enter] Expand  [Q] Quit      │
└─────────────────────────────────────────────────────────────────┘
```

### 3.4 TUI 界面设计 — 传输统计

```
┌─ [0] Discovery ─ [1] Topics ─ [2] Match ─ [3] Variables ─ ────┐
│ Topic Transfer Statistics                                       │
│                                                                 │
│ ┌─ Transfer Stats Table ──────────────────────────────────────┐ │
│ │ Topic / Writer     │ Pkts │Complete│Frag│Lost%│NACK%│Rate   │ │
│ ├────────────────────┼──────┼────────┼────┼─────┼─────┼───────┤ │
│ │ bucket_0           │      │        │    │     │     │       │ │
│ │  └ pubNode:03030.. │ 1234 │ 1200   │ 34 │ 0.2%│ 0.1%│ 42/s  │ │
│ │ bucket_1           │      │        │    │     │     │       │ │
│ │  └ pubNode:03040.. │ 567  │ 567    │ 0  │ 0.0%│ 0.0%│ 21/s  │ │
│ │ tableDefine        │      │        │    │     │     │       │ │
│ │  └ pubNode:c2030.. │ 45   │ 45     │ 0  │ 0.0%│ 0.0%│ 0.5/s │ │
│ └────────────────────┴──────┴────────┴────┴─────┴─────┴───────┘ │
│                                                                 │
│ ┌─ SN Loss Detail (selected) ─────────────────────────────────┐ │
│ │ Writer: pubNode:03030000 (bucket_0)                          │ │
│ │ Expected SN Range: 1 — 5000                                  │ │
│ │ Received: 4990 │ Lost: 10 │ Duplicate: 2                    │ │
│ │ Loss Rate: 0.20%                                             │ │
│ │                                                              │ │
│ │ Lost SN Ranges:                                              │ │
│ │   [1234, 1236) — 2 packets                                   │ │
│ │   [2345, 2348) — 3 packets                                   │ │
│ │   [3456, 3460) — 4 packets                                   │ │
│ │   [4567] — 1 packet                                          │ │
│ │                                                              │ │
│ │ NACK History (last 10):                                      │ │
│ │   14:35:22 NACK SN=1234 from subNode41975                    │ │
│ │   14:35:21 NACK SN=2345 from subNode2                        │ │
│ └──────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ Stats: 5 Topics │ 8 Endpoints │ 6 Matched │ 2 Unmatched        │
│ [Tab] Switch Panel  [↑↓] Select  [Space] Expand  [Q] Quit      │
└─────────────────────────────────────────────────────────────────┘
```

### 3.5 SEDP 报文解析

SEDP 报文用于发现 Endpoint (DataWriter/DataReader):
- **SEDP Publication Writer**: Entity ID = `0x000300c2` (ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER)
- **SEDP Subscription Writer**: Entity ID = `0x000400c2` (ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER)

需要解析的 PID:

| PID | 值 | 类型 | 说明 |
|-----|-----|------|------|
| PID_TOPIC_NAME | 0x0005 | string | Topic 名称 |
| PID_TYPE_NAME | 0x0007 | string | 类型名称 |
| PID_ENDPOINT_GUID | 0x005A | GUID_t | Endpoint GUID |
| PID_RELIABILITY | 0x001A | ReliabilityQosPolicy | 可靠性 |
| PID_DURABILITY | 0x001D | DurabilityQosPolicy | 持久性 |
| PID_OWNERSHIP | 0x001F | OwnershipQosPolicy | 所有权 |
| PID_LIVELINESS | 0x001B | LivelinessQosPolicy | 活跃性 |
| PID_DEADLINE | 0x0004 | DeadlineQosPolicy | 截止时间 |
| PID_HISTORY | 0x0040 | HistoryQosPolicy | 历史深度 |

### 3.6 解析代码扩展

```cpp
namespace rtps {

// SEDP 解析结果
struct SedpParseResult {
    bool valid = false;
    bool is_publication;          // true=Writer, false=Reader
    std::string endpoint_guid;    // 8字节 entity (hex)
    std::string participant_guid; // 所属 participant guid prefix
    std::string topic_name;
    std::string type_name;

    // QoS
    struct Qos {
        int reliability = 0;      // 0=BEST_EFFORT, 1=RELIABLE
        int durability = 0;       // 0=VOLATILE, 1=TRANSIENT_LOCAL
        int ownership = 0;        // 0=SHARED, 1=EXCLUSIVE
        int liveliness = 0;       // 0=AUTOMATIC, 1=MANUAL
        int deadline_sec = -1;    // -1=INFINITE
        int history_depth = 1;
    } qos;
};

// 解析 SEDP 报文
SedpParseResult parse_sedp(const uint8_t* payload, int submsg_offset,
                           int submsg_len, bool is_be, bool is_pub);

} // namespace rtps
```

### 3.7 匹配算法

```cpp
class MatchAnalyzer {
public:
    // 分析所有 Topic 的匹配情况
    std::vector<TopicMatch> analyze_matches(
        const std::map<std::string, EndpointInfo>& endpoints);

    // 检查 QoS 兼容性
    static bool check_qos_compatible(const EndpointInfo::QosInfo& pub_qos,
                                     const EndpointInfo::QosInfo& sub_qos,
                                     std::string& reason);

private:
    // QoS 兼容性规则
    static bool check_reliability(int pub, int sub);
    static bool check_durability(int pub, int sub);
    static bool check_ownership(int pub, int sub);
};
```

**QoS 兼容性规则:**

| QoS | Publisher | Subscriber | 兼容? |
|-----|-----------|------------|-------|
| Reliability | RELIABLE | BEST_EFFORT | ✓ |
| Reliability | BEST_EFFORT | RELIABLE | ✗ |
| Durability | VOLATILE | TRANSIENT_LOCAL | ✗ |
| Durability | TRANSIENT_LOCAL | VOLATILE | ✓ |
| Ownership | EXCLUSIVE | SHARED | ✗ |
| Ownership | SHARED | EXCLUSIVE | ✓ |

### 3.8 传输统计追踪

```cpp
class TransferStatsTracker {
public:
    // 更新 SN (从 DATA 子消息)
    void update_sn(const std::string& writer_guid, int64_t sn);

    // 记录 ACK (从 ACKNACK 子消息)
    void record_ack(const std::string& writer_guid, int64_t sn);

    // 记录 NACK (从 ACKNACK 子消息, bitmap 中为 0 的位)
    void record_nack(const std::string& writer_guid, int64_t sn);

    // 记录 Heartbeat
    void record_heartbeat(const std::string& writer_guid,
                         int64_t first_sn, int64_t last_sn);

    // 记录分片
    void record_fragment(const std::string& writer_guid, int32_t frag_num,
                        int32_t num_fragments);

    // 记录 GAP
    void record_gap(const std::string& writer_guid, int64_t gap_start,
                   int64_t gap_end);

    // 获取统计
    TopicTransferStats get_stats(const std::string& writer_guid) const;

    // 获取所有统计
    std::map<std::string, TopicTransferStats> get_all_stats() const;

private:
    mutable std::mutex mtx_;

    struct WriterState {
        // SN 追踪
        int64_t last_sn = 0;
        std::set<int64_t> received_sns;
        std::set<int64_t> lost_sns;

        // 分片追踪
        struct FragState {
            int32_t total_frags = 0;
            std::set<int32_t> received_frags;
        };
        std::map<int64_t, FragState> frag_map;  // SN -> FragState

        // 计数器
        uint64_t total_packets = 0;
        uint64_t complete_packets = 0;
        uint64_t fragmented_packets = 0;
        uint64_t total_fragments = 0;
        uint64_t ack_count = 0;
        uint64_t nack_count = 0;
        uint64_t heartbeat_count = 0;
        uint64_t gap_count = 0;
        uint64_t total_bytes = 0;

        // 速率
        RateMeter packet_rate;
        RateMeter byte_rate;
    };

    std::map<std::string, WriterState> writers_;
};
```

### 3.9 ACKNACK 解析

ACKNACK 子消息结构:
```
+-------------------+-------------------+
| readerId (4 bytes)| writerId (4 bytes)|
+-------------------+-------------------+
| readerSNState (8 bytes)                |
|   - bitmapBase (high 32 + low 32)     |
+----------------------------------------+
| count (4 bytes)                        |
+----------------------------------------+
| bitmap (variable length)               |
|   - 0 = NACK, 1 = ACK                 |
+----------------------------------------+
```

```cpp
struct AcknackInfo {
    std::string reader_id;
    std::string writer_id;
    int64_t bitmap_base;       // 期望的下一个 SN
    std::vector<bool> bitmap;  // true=ACK, false=NACK
    int32_t count;             // 序列号
};

AcknackInfo parse_acknack(const uint8_t* payload, int offset, int length, bool is_be);
```

---

## 4. JSON 数据导出

### 4.1 功能描述

后台定时生成 JSON 格式的监控快照，可通过以下方式输出:
1. **文件输出** — 写入指定文件，Web 前端轮询读取
2. **HTTP API** — 内置轻量 HTTP 服务器，提供 REST API
3. **WebSocket** — 实时推送更新

### 4.2 JSON Schema

```json
{
  "version": "4.0",
  "timestamp": 1692436222.123,
  "uptime_sec": 120.5,

  "global_stats": {
    "total_packets": 50000,
    "rtps_packets": 45000,
    "packet_rate": 42.5,
    "byte_rate": 12500.0,
    "domain_count": 1
  },

  "participants": [
    {
      "guid_prefix": "010f00786104000000000000",
      "domain_id": 77,
      "name": "pubNode",
      "is_alive": true,
      "first_seen": 1692436101.0,
      "last_seen": 1692436221.0,
      "pdp_count": 120,
      "metatraffic_unicast": [
        {"kind": 1, "port": 36140, "address": "192.168.1.100"}
      ],
      "metatraffic_multicast": [
        {"kind": 1, "port": 7400, "address": "239.255.0.1"}
      ],
      "endpoints": [
        {
          "guid": "03030000",
          "topic": "dsf/var/data/transfer/bucket_0",
          "type": "TableDataTransfer",
          "is_publisher": true,
          "qos": {
            "reliability": 1,
            "durability": 0
          }
        }
      ],
      "packet_count": 25000
    }
  ],

  "topics": [
    {
      "name": "dsf/var/data/transfer/bucket_0",
      "type": "TableDataTransfer",
      "writers": [
        {
          "guid": "03030000",
          "participant": "pubNode",
          "stats": {
            "total_packets": 1234,
            "complete_packets": 1200,
            "fragmented_packets": 34,
            "total_fragments": 156,
            "sn_expected": 5000,
            "sn_received": 4990,
            "sn_lost": 10,
            "loss_rate": 0.002,
            "ack_count": 4990,
            "nack_count": 5,
            "retransmission_rate": 0.001,
            "packets_per_sec": 42.0,
            "bytes_per_sec": 12000.0
          }
        }
      ],
      "readers": [
        {
          "guid": "03040000",
          "participant": "subNode41975"
        }
      ],
      "match_status": {
        "matched_pairs": 2,
        "unmatched_publishers": 0,
        "unmatched_subscribers": 1,
        "qos_mismatches": 0
      }
    }
  ],

  "match_matrix": {
    "dsf/var/data/transfer/bucket_0": {
      "pub": ["pubNode:03030000"],
      "sub": ["subNode41975:03040000", "subNode2:03050000", "subNode3:03060000"],
      "matched": [
        {"pub": "pubNode:03030000", "sub": "subNode41975:03040000", "qos_ok": true},
        {"pub": "pubNode:03030000", "sub": "subNode2:03050000", "qos_ok": true}
      ],
      "unmatched": [
        {"endpoint": "subNode3:03060000", "reason": "No matching writer"}
      ]
    }
  },

  "alerts": [
    {
      "level": "warning",
      "message": "Participant subNode3 inactive for 5s",
      "timestamp": 1692436220.0
    },
    {
      "level": "error",
      "message": "Topic bucket_0: 3 subscribers unmatched",
      "timestamp": 1692436219.0
    }
  ]
}
```

### 4.3 JSON Exporter 实现

```cpp
class JsonExporter {
public:
    struct Config {
        bool enable_file = true;
        std::string file_path = "/tmp/monitor_snapshot.json";
        int file_interval_ms = 1000;  // 文件写入间隔

        bool enable_http = false;
        int http_port = 8080;

        bool enable_websocket = false;
        int websocket_port = 8081;
    };

    JsonExporter(const Config& config, MetricsEngine& metrics);
    ~JsonExporter();

    void start();
    void stop();

    // 手动触发导出
    std::string generate_json() const;

private:
    void file_writer_loop();
    void http_server_loop();
    void websocket_loop();

    Config config_;
    MetricsEngine& metrics_;
    std::atomic<bool> running_;

    std::thread file_thread_;
    std::thread http_thread_;
    std::thread websocket_thread_;
};

// JSON 生成辅助
class JsonBuilder {
public:
    static std::string snapshot_to_json(const Snapshot& snap);

private:
    static std::string participant_to_json(const ParticipantInfo& p);
    static std::string topic_to_json(const TopicInfo& t,
                                     const std::vector<TopicTransferStats>& stats);
    static std::string match_to_json(const TopicMatch& m);
    static std::string escape_string(const std::string& s);
};
```

### 4.4 HTTP API 设计

```
GET /api/v1/snapshot          — 完整快照
GET /api/v1/participants      — 参与者列表
GET /api/v1/topics            — Topic 列表
GET /api/v1/topics/:name      — 单个 Topic 详情
GET /api/v1/match             — 匹配矩阵
GET /api/v1/stats             — 全局统计
GET /api/v1/alerts            — 告警列表

WebSocket /ws/live            — 实时更新推送
```

---

## 5. 实现计划

### Phase 1: PDP 深度解析 (Day 1-2)

| 任务 | 文件 | 说明 |
|------|------|------|
| 扩展 RTPS 解析器 | `rtps_parser.hpp/cpp` | 新增 `parse_spdp_full()` |
| 新增 ParticipantInfo | `metrics.hpp` | 数据结构定义 |
| 更新 MetricsEngine | `metrics.cpp` | `on_pdp_packet()`, 超时检测 |
| 修改 PcapWorker | `pcap_worker.cpp` | 调用新解析函数 |
| 更新 Discovery 页面 | `page_discovery.cpp` | 显示 Participant 列表 |

### Phase 2: SEDP 解析与匹配 (Day 3-4)

| 任务 | 文件 | 说明 |
|------|------|------|
| SEDP 解析器 | `rtps_parser.hpp/cpp` | `parse_sedp()` |
| EndpointInfo 结构 | `metrics.hpp` | 数据结构定义 |
| 匹配分析器 | `match_analyzer.hpp/cpp` | 新增文件 |
| Topic 匹配页面 | `page_match.hpp/cpp` | 新增 TUI 页面 |
| 传输统计追踪 | `transfer_stats.hpp/cpp` | SN/ACK/NACK 追踪 |

### Phase 3: 统计与 UI (Day 5)

| 任务 | 文件 | 说明 |
|------|------|------|
| ACKNACK 解析 | `rtps_parser.hpp/cpp` | 新增解析函数 |
| 传输统计页面 | `page_stats.hpp/cpp` | 新增 TUI 页面 |
| Stats 页面集成 | `app.cpp` | 添加新 Tab |

### Phase 4: JSON 导出 (Day 6-7)

| 任务 | 文件 | 说明 |
|------|------|------|
| JSON Builder | `json_builder.hpp/cpp` | JSON 生成 |
| File Exporter | `json_exporter.hpp/cpp` | 文件输出 |
| HTTP Server | `http_server.hpp/cpp` | 可选，轻量 HTTP |
| WebSocket | `ws_server.hpp/cpp` | 可选，实时推送 |
| 命令行参数 | `main.cpp` | 添加导出配置 |

---

## 6. 命令行接口

```bash
# 基本用法 (与 v3 兼容)
sudo ./monitor -i eth0 -d 77

# 新增参数
sudo ./monitor -i eth0 -d 77 \
  --json-file /tmp/monitor.json \        # JSON 文件输出
  --json-interval 1000 \                 # 输出间隔 (ms)
  --http-port 8080 \                     # HTTP API 端口
  --ws-port 8081 \                       # WebSocket 端口
  --pdp-timeout 5 \                      # PDP 超时 (秒)
  --log-level info                       # 日志级别
```

---

## 7. 测试用例

### 7.1 PDP 解析测试

```cpp
TEST(RtpsParser, ParseSpdpFull) {
    // 构造 SPDP 报文
    uint8_t packet[512] = { /* ... */ };

    auto result = rtps::parse_spdp_full(packet, 0, sizeof(packet), false);

    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.domain_id, 77);
    EXPECT_EQ(result.participant_name, "pubNode");
    EXPECT_EQ(result.guid_prefix, "010f00786104000000000000");
    EXPECT_EQ(result.metatraffic_unicast.size(), 1);
    EXPECT_EQ(result.metatraffic_unicast[0].port, 36140);
}
```

### 7.2 匹配分析测试

```cpp
TEST(MatchAnalyzer, BasicMatch) {
    std::map<std::string, EndpointInfo> endpoints;

    // 添加 Publisher
    EndpointInfo pub;
    pub.endpoint_guid = "03030000";
    pub.topic_name = "test_topic";
    pub.is_publisher = true;
    pub.qos.reliability = 1;  // RELIABLE
    endpoints["03030000"] = pub;

    // 添加 Subscriber
    EndpointInfo sub;
    sub.endpoint_guid = "03040000";
    sub.topic_name = "test_topic";
    sub.is_publisher = false;
    sub.qos.reliability = 1;  // RELIABLE
    endpoints["03040000"] = sub;

    MatchAnalyzer analyzer;
    auto matches = analyzer.analyze_matches(endpoints);

    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0].matched_pairs.size(), 1);
    EXPECT_EQ(matches[0].unmatched_endpoints.size(), 0);
}
```

### 7.3 SN 追踪测试

```cpp
TEST(TransferStatsTracker, LossDetection) {
    TransferStatsTracker tracker;

    tracker.update_sn("writer1", 1);
    tracker.update_sn("writer1", 2);
    tracker.update_sn("writer1", 4);  // SN 3 丢失
    tracker.update_sn("writer1", 5);

    auto stats = tracker.get_stats("writer1");
    EXPECT_EQ(stats.sn_received, 4);
    EXPECT_EQ(stats.sn_lost, 1);
    EXPECT_NEAR(stats.loss_rate(), 0.2, 0.01);
}
```

---

## 8. 依赖与构建

### 8.1 新增依赖

| 库 | 版本 | 用途 | 可选 |
|----|------|------|------|
| nlohmann/json | 3.11+ | JSON 生成 | 否 |
| cpp-httplib | 0.14+ | HTTP 服务器 | 是 |
| libwebsockets | 4.3+ | WebSocket | 是 |

### 8.2 CMakeLists.txt 修改

```cmake
# JSON 库 (header-only)
include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

# HTTP 库 (可选)
option(ENABLE_HTTP "Enable HTTP API" OFF)
if(ENABLE_HTTP)
    FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.14.0)
    FetchContent_MakeAvailable(httplib)
    add_definitions(-DENABLE_HTTP)
endif()

target_link_libraries(monitor PRIVATE
    # ... existing ...
    nlohmann_json::nlohmann_json
    $<IF:$<BOOL:${ENABLE_HTTP}>,httplib::httplib,>
)
```

---

## 9. 注意事项

### 9.1 性能考虑

1. **Participant 追踪** — 使用 `std::map` 而非 `std::unordered_map`，因为 GUID prefix 数量有限 (< 100)
2. **SN 追踪** — 使用环形缓冲区存储最近的 SN，避免内存无限增长
3. **JSON 生成** — 每次生成完整快照，避免增量更新的复杂性
4. **匹配计算** — 缓存匹配结果，仅在 Endpoint 变化时重新计算

### 9.2 线程安全

1. **MetricsEngine** — 所有写操作持有 `mtx_`
2. **TransferStatsTracker** — 独立的 mutex，避免与 MetricsEngine 竞争
3. **JSON Exporter** — 使用 snapshot 的拷贝，避免长时间持锁

### 9.3 兼容性

1. **向后兼容** — 保留所有 v3 的命令行参数和功能
2. **Layer 2 兼容** — 保留 PubTableDefine/SubTableRegister 订阅
3. **TUI 兼容** — 新增 Tab 页面，不改变现有页面布局

---

## 10. 验收标准

### 10.1 PDP Discovery

- [ ] 能够从 SPDP 报文中解析出 Domain ID、GUID、Name
- [ ] 能够解析 Locator (unicast/multicast)
- [ ] 5秒超时自动标记为 Inactive
- [ ] TUI 正确显示 Participant 列表和状态

### 10.2 Topic Match

- [ ] 能够从 SEDP 报文中解析出 Endpoint 信息
- [ ] 正确匹配 Publisher 和 Subscriber
- [ ] QoS 兼容性检查正确
- [ ] TUI 直观显示匹配/未匹配状态

### 10.3 Transfer Stats

- [ ] SN 丢包检测正确
- [ ] ACK/NACK 统计正确
- [ ] 分片包统计正确
- [ ] 丢包率、重传率计算正确

### 10.4 JSON Export

- [ ] JSON 格式符合 Schema 定义
- [ ] 文件输出定时写入
- [ ] HTTP API 正确响应
- [ ] WebSocket 实时推送

---

## 附录 A: RTPS 子消息 ID 参考

| ID | 名称 | 说明 |
|----|------|------|
| 0x06 | ACKNACK | 确认/否认 |
| 0x07 | HEARTBEAT | 心跳 |
| 0x08 | GAP | 间隔 |
| 0x09 | INFO_TS | 时间戳 |
| 0x0e | INFO_DST | 目标 GUID |
| 0x15 | DATA | 数据 |
| 0x16 | DATA_FRAG | 数据分片 |

## 附录 B: 常用 PID 参考

| PID | 值 | 说明 |
|-----|-----|------|
| PID_SENTINEL | 0x0001 | 参数列表结束 |
| PID_DOMAIN_ID | 0x000F | Domain ID |
| PID_TOPIC_NAME | 0x0005 | Topic 名称 |
| PID_TYPE_NAME | 0x0007 | 类型名称 |
| PID_RELIABILITY | 0x001A | 可靠性 QoS |
| PID_DURABILITY | 0x001D | 持久性 QoS |
| PID_ENDPOINT_GUID | 0x005A | Endpoint GUID |
| PID_PARTICIPANT_GUID | 0x0050 | Participant GUID |
| PID_PARTICIPANT_NAME | 0x0060 | Participant 名称 |
| PID_METATRAFFIC_UNICAST | 0x0032 | 元数据单播定位器 |
| PID_METATRAFFIC_MULTICAST | 0x0033 | 元数据组播定位器 |

---

**文档结束**

*此文档可直接交给 AI 开发，每个 Phase 的任务清晰明确，数据结构和接口已定义。*
