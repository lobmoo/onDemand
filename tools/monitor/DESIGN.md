# OnDemand DDS Monitor 设计文档

> 版本: v1.0
> 日期: 2026-08-06
> 作者: Claude + wwk

---

## 1. 背景与目标

### 1.1 现状问题

OnDemand 是一个高性能 pub-sub 系统，管理 10 万+ 变量的 DDS 分发。当前调试完全依赖日志 (`ONDEMANDLOG`)，存在以下问题：

- **无网络层可视性**：无法观察 RTPS 协议层面的包交互（HEARTBEAT、ACKNACK、GAP）
- **无实时指标**：吞吐量、时延抖动、心跳丢失率等关键指标无法量化
- **无变量级监控**：不知道当前 pub 发布了哪些变量、sub 订阅了什么频率
- **调试效率低**：出问题只能翻日志，没有实时仪表盘

### 1.2 目标

构建一个**分层、可交互**的监控工具，覆盖从网络包到应用变量的全链路观测：

```
网络层 (pcap)  →  协议层 (RTPS)  →  应用层 (DDS topic)  →  终端展示 (TUI)
```

---

## 2. 系统架构

### 2.1 三层架构

```
┌─────────────────────────────────────────────────────────┐
│              Layer 3: Interactive TUI                    │
│           rich + textual 交互终端界面                    │
│         F1: Network  F2: Topics  F3: Vars  F4: Perf     │
├─────────────────────────────────────────────────────────┤
│              Layer 2: DDS Application Layer              │
│          fastdds Python 绑定, 订阅 DDS topic             │
│     tableDefine / subTableRegister / bucket_0..19        │
├─────────────────────────────────────────────────────────┤
│              Layer 1: Network Capture Layer              │
│          scapy 抓 UDP 包, 解析 RTPS 子消息               │
│     HEARTBEAT / ACKNACK / GAP / DATA / INFO_TS          │
└─────────────────────────────────────────────────────────┘
```

### 2.2 数据流

```
                    ┌──────────────┐
                    │  网卡 (eth0) │
                    └──────┬───────┘
                           │ raw packets
                    ┌──────▼───────┐
                    │  Layer 1     │
                    │  rtps_parser │──→ RTPS 统计 (HB/ACK/GAP/DATA)
                    │  pcap_worker │──→ Topic 分布 (从 DATA 子消息提取)
                    └──────┬───────┘
                           │ parsed metrics
                    ┌──────▼───────┐
                    │  Layer 2     │
                    │  dds_worker  │──→ 变量定义 (tableDefine)
                    │              │──→ 订阅请求 (subTableRegister)
                    │              │──→ 数据流统计 (bucket_N)
                    └──────┬───────┘
                           │ application metrics
                    ┌──────▼───────┐
                    │  Layer 3     │
                    │  tui_app     │──→ 交互终端渲染
                    └──────────────┘
```

### 2.3 目录结构

```
tools/monitor/
├── DESIGN.md               # 本文档
├── main.py                 # 入口, CLI 参数解析
├── config.py               # 配置 (domain_id, topic 名, 网卡等)
├── rtps_parser.py          # RTPS 协议解析器 (子消息级)
├── layer1_pcap.py          # 抓包层: scapy + RTPS 解析 + 统计
├── layer2_dds.py           # DDS 层: fastdds 订阅 + 应用指标
├── layer3_tui.py           # TUI 层: 交互终端界面
├── metrics.py              # 指标计算引擎 (滑动窗口)
└── requirements.txt        # Python 依赖
```

---

## 3. Layer 1: 网络抓包层

### 3.1 技术选型

| 组件 | 选型 | 理由 |
|------|------|------|
| 抓包 | scapy | Python 原生，支持 BPF 过滤，可扩展协议解析 |
| RTPS 解析 | 自研 (rtps_parser.py) | 无现成 Python RTPS 解析库，需手动解析子消息 |

### 3.2 RTPS 协议解析

#### 3.2.1 RTPS 包结构

```
┌─────────────────────────────────────────┐
│ RTPS Header (20 bytes)                  │
│   magic: "R.T.P.S" (4 bytes)           │
│   protocol_version: 2.x                 │
│   vendor_id: eProsima (0x010F)         │
│   guid_prefix: 12 bytes                 │
├─────────────────────────────────────────┤
│ Submessage 1                            │
│   submsg_id (1 byte)                    │
│   flags (1 byte, endianness bit)        │
│   length (2 bytes)                      │
│   payload...                            │
├─────────────────────────────────────────┤
│ Submessage 2                            │
│   ...                                   │
└─────────────────────────────────────────┘
```

#### 3.2.2 子消息类型

| ID | 名称 | 用途 | 监控意义 |
|----|------|------|----------|
| `0x09` | INFO_TS | 时间戳标记 | 消息时间基准 |
| `0x15` | DATA | 数据消息 | 核心：携带变量数据，可提取 topic name |
| `0x07` | HEARTBEAT | 心跳 | writer 告知 reader 已发序号范围 |
| `0x06` | ACKNACK | 确认/否认 | reader 告知 writer 已收/缺失序号 |
| `0x08` | GAP | 缺口通知 | writer 告知 reader 某段序号不可用 |
| `0x0e` | INFO_DST | 目标 GUID | 指定后续子消息的目标 reader |

#### 3.2.3 Topic Name 提取

DATA 子消息的 inline QoS 参数列表中包含 `PID_TOPIC_NAME (0x0005)`：

```
DATA 子消息:
  extraFlags (2 bytes)
  octetsToInlineQoS (2 bytes)
  readerId (4 bytes)
  writerId (4 bytes)
  sequenceNumber (8 bytes)
  inline QoS parameters:
    PID_TOPIC_NAME (0x0005): "dsf/var/data/transfer/bucket_0" + padding
    PID_SENTINEL (0x0001): end
  serialized payload
```

### 3.3 采集指标

#### 3.3.1 包级别统计

| 指标 | 类型 | 单位 | 说明 |
|------|------|------|------|
| `pkt_count` | 计数器 | - | 总包数 |
| `pkt_rate` | 速率 | pkt/s | 包速率 (1s 滑动窗口) |
| `byte_count` | 计数器 | bytes | 总字节数 |
| `throughput` | 速率 | MB/s | 吞吐量 (1s 滑动窗口) |

#### 3.3.2 RTPS 协议统计

| 指标 | 类型 | 单位 | 说明 |
|------|------|------|------|
| `hb_count` | 计数器 | - | HEARTBEAT 包数 |
| `ack_count` | 计数器 | - | ACKNACK 包数 |
| `gap_count` | 计数器 | - | GAP 消息数 |
| `data_count` | 计数器 | - | DATA 消息数 |
| `hb_loss_rate` | 比率 | % | 心跳丢失率 = 1 - (ACK/HEARTBEAT) |
| `resend_rate` | 比率 | % | 重发率 = GAP / DATA |

#### 3.3.3 Topic 分布

| 指标 | 类型 | 说明 |
|------|------|------|
| `topic_pkt_count[topic]` | 计数器 | 各 topic 的包数 |
| `topic_byte_count[topic]` | 计数器 | 各 topic 的字节数 |
| `topic_pct[topic]` | 比率 | 各 topic 占比 |

### 3.4 BPF 过滤

```python
# scapy BPF filter: 只抓 RTPS 相关 UDP 包
BPF_FILTER = "udp and (port 7400 or port 7401)"
```

- 端口 7400: RTPS discovery multicast (239.255.0.1)
- 端口 7401: RTPS user data multicast (239.255.0.1)

---

## 4. Layer 2: DDS 应用层监控

### 4.1 技术选型

| 组件 | 选型 | 理由 |
|------|------|------|
| DDS 绑定 | fastdds (pip install fastdds) | eProsima 官方 Python 绑定 |
| Domain ID | 66 | 与 onDemand 系统一致 |

### 4.2 订阅 Topic

#### 4.2.1 变量定义 Topic

```
Topic:  dsf/sys/var/tableDefine
Type:   DSF::Var::PubTableDefine
QoS:    RELIABLE, TRANSIENT_LOCAL, KEEP_LAST(1)
```

**提取信息：**
- `nodeName`: pub 节点名
- `name`: table 名 (bucket 名)
- `varDefines[]`: 变量列表，每个包含 `id` (varHash) 和 `VarRequest.varDefine`

**监控指标：**
- `pub_nodes`: 活跃 pub 节点集合
- `var_count`: 当前已定义变量总数
- `bucket_var_count[20]`: 各 bucket 变量数
- `var_table`: 变量详情 (name, nodeName, bucket, id)

#### 4.2.2 订阅请求 Topic

```
Topic:  dsf/message/commandRequest/subTableRegister
Type:   DSF::Message::SubTableRegister
QoS:    RELIABLE, TRANSIENT_LOCAL, KEEP_LAST(64)
```

**提取信息：**
- `nodeName`: sub 节点名
- `varFreqs[]`: 变量名 → 频率 (ms)
- `tableName`: 订阅的 table 名

**监控指标：**
- `sub_nodes`: 活跃 sub 节点集合
- `sub_freq_dist`: 订阅频率分布 (10ms / 100ms / 1s / ...)
- `sub_var_count`: 各 sub 订阅的变量数

#### 4.2.3 数据传输 Topic

```
Topic:  dsf/var/data/transfer/bucket_N  (N = 0..19)
Type:   DSF::Var::TableDataTransfer
QoS:    DEFAULT (Best Effort)
```

**提取信息：**
- `mask`: Roaring64Map 序列化字节 → 反序列化得到 varHash 集合
- `varData[]`: 变量数据数组 (与 Roaring64Map 迭代顺序一一对应)
- `timestamp`: 发布时间戳 (tv_sec + tv_nsec)
- `blobType`: 序列化类型

**监控指标：**
- `data_msg_rate[bucket]`: 各 bucket 消息速率 (msg/s)
- `mask_var_count[bucket]`: 单次消息携带的变量数 (Roaring64Map cardinality)
- `var_data_size[bucket]`: 变量数据大小分布 (bytes)
- `jitter[bucket]`: 消息时间戳抖动 (ms)
- `throughput[bucket]`: 各 bucket 吞吐量 (bytes/s)

### 4.3 Roaring64Map 反序列化

```python
import roaringpy  # 或手动实现

def deserialize_mask(mask_bytes: bytes) -> list[int]:
    """反序列化 Roaring64Map，返回 varHash 列表 (升序)"""
    roar = roaringpy.Roaring64Map.read(mask_bytes)
    return list(roar)
```

> 备选：如果没有 roaringpy，可以只统计 `len(varData)` 作为变量数近似值。

### 4.4 Jitter 计算

```python
class JitterCalculator:
    """滑动窗口 jitter 计算器"""
    def __init__(self, window_size=100):
        self.window_size = window_size
        self.timestamps = []

    def add(self, ts_sec: float, ts_nsec: float):
        ts = ts_sec + ts_nsec * 1e-9
        self.timestamps.append(ts)
        if len(self.timestamps) > self.window_size:
            self.timestamps.pop(0)

    def jitter_ms(self) -> float:
        """返回标准差 jitter (ms)"""
        if len(self.timestamps) < 2:
            return 0.0
        deltas = [self.timestamps[i] - self.timestamps[i-1]
                  for i in range(1, len(self.timestamps))]
        mean = sum(deltas) / len(deltas)
        variance = sum((d - mean) ** 2 for d in deltas) / len(deltas)
        return (variance ** 0.5) * 1000  # 转换为 ms
```

---

## 5. Layer 3: TUI 交互终端

### 5.1 技术选型

| 组件 | 选型 | 理由 |
|------|------|------|
| 渲染 | rich | 终端表格、面板、进度条、颜色 |
| 交互 | textual | 键盘事件、视图切换、滚动 |
| 备选 | curses + rich | 如果 textual 依赖太重 |

### 5.2 界面布局

#### 5.2.1 主框架

```
┌─ OnDemand DDS Monitor ──────────────────────────────────────────────┐
│ [F1] Network   [F2] Topics   [F3] Variables   [F4] Performance     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│                     (当前视图内容区域)                               │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│ [Status] Running | Uptime: 00:05:32 | Domain: 66 | eth0            │
│ [Q] Quit  [R] Reset  [P] Pause  [↑↓] Scroll                       │
└─────────────────────────────────────────────────────────────────────┘
```

#### 5.2.2 F1: Network 视图

```
┌─ Network Overview ─────────────────────┐ ┌─ RTPS Protocol ───────────────────┐
│ Interface: eth0                         │ │ SubMsg Type    │ Count    │ Rate   │
│ Domain: 66                              │ │─────────────── │───────── │─────── │
│ Uptime: 00:05:32                        │ │ HEARTBEAT      │ 45,678   │ 76/s   │
│                                         │ │ ACKNACK        │ 43,210   │ 72/s   │
│ Total Packets:   1,234,567              │ │ DATA           │ 1,100,000│ 1833/s │
│ Packet Rate:     12,345 pkt/s          │ │ GAP            │ 123      │ 0.2/s  │
│ Total Bytes:     1.23 GB               │ │ INFO_TS        │ 55,000   │ 92/s   │
│ Throughput:      1.2 MB/s              │ │ INFO_DST       │ 50,000   │ 83/s   │
│                                         │ │                │          │        │
│                                         │ │ HB Loss Rate:  │ 0.02%            │
│                                         │ │ Resend Rate:   │ 0.01%            │
└─────────────────────────────────────────┘ └───────────────────────────────────┘

┌─ Topic Distribution ──────────────────────────────────────────────────────────┐
│ Topic                              │ Packets    │ Bytes      │ Pct    │ Rate  │
│─────────────────────────────────── │─────────── │─────────── │─────── │────── │
│ dsf/var/data/transfer/bucket_0     │ 120,000    │ 120 MB     │ 9.7%   │ 200/s │
│ dsf/var/data/transfer/bucket_1     │ 115,000    │ 115 MB     │ 9.3%   │ 192/s │
│ ...                                │ ...        │ ...        │ ...    │ ...   │
│ dsf/sys/var/tableDefine            │ 40         │ 40 KB      │ 0.0%   │ 0/s   │
│ dsf/message/.../subTableRegister   │ 10         │ 1 KB       │ 0.0%   │ 0/s   │
└───────────────────────────────────────────────────────────────────────────────┘
```

#### 5.2.3 F2: Topics 视图

```
┌─ TableDefine (pub → sub) ─────────────────────────────────────────────────────┐
│ Timestamp           │ Pub Node    │ Table          │ Var Count │ Latest Vars  │
│─────────────────────│─────────────│────────────────│───────────│──────────────│
│ 14:09:50.123        │ pub1        │ bucket_0       │ 5,234     │ var0,var1... │
│ 14:09:50.155        │ pub1        │ bucket_1       │ 5,100     │ var3,var7... │
│ 14:10:05.789        │ pub2        │ bucket_0       │ 3,000     │ var100...    │
└───────────────────────────────────────────────────────────────────────────────┘

┌─ SubTableRegister (sub → pub) ────────────────────────────────────────────────┐
│ Timestamp           │ Sub Node    │ Table          │ Var Count │ Freq Dist    │
│─────────────────────│─────────────│────────────────│───────────│──────────────│
│ 14:09:51.001        │ sub1        │ bucket_0       │ 2,000     │ 10ms:500,... │
│ 14:09:51.002        │ sub1        │ bucket_1       │ 1,800     │ 100ms:1800   │
└───────────────────────────────────────────────────────────────────────────────┘
```

#### 5.2.4 F3: Variables 视图

```
┌─ Pub Nodes ──────┐ ┌─ Sub Nodes ──────┐ ┌─ Bucket Distribution ─────────────┐
│ pub1 (active)    │ │ sub1 (active)    │ │ Bucket │ Vars    │ Pct   │ Bar     │
│ pub2 (active)    │ │ sub2 (active)    │ │────────│─────────│───────│──────── │
│                  │ │                  │ │   0    │ 5,234   │ 5.2%  │ ████    │
│ Total vars:      │ │ Subscriptions:   │ │   1    │ 5,100   │ 5.1%  │ ████    │
│   100,000        │ │   50,000         │ │  ...   │  ...    │  ...  │  ...    │
└──────────────────┘ └──────────────────┘ │  19    │ 5,050   │ 5.0%  │ ████    │
                                          └──────────────────────────────────┘

┌─ Variable List (top 20 by frequency) ─────────────────────────────────────────┐
│ varHash          │ Pub Node │ Bucket │ Sub Freq │ Sub Count │ Data Size      │
│──────────────────│──────────│────────│──────────│───────────│────────────────│
│ 0x1a2b3c4d5e6f.. │ pub1     │   0    │ 10 ms    │ 3         │ 64 bytes       │
│ 0x2b3c4d5e6f7a.. │ pub1     │   0    │ 100 ms   │ 5         │ 128 bytes      │
│ ...              │ ...      │  ...   │ ...      │ ...       │ ...            │
└───────────────────────────────────────────────────────────────────────────────┘
```

#### 5.2.5 F4: Performance 视图

```
┌─ Jitter (ms) ────────────────────────────────────────────────────────────────┐
│ Bucket │ Mean    │ StdDev  │ P99     │ Min     │ Max     │ Samples │ Sparkline│
│────────│─────────│─────────│─────────│─────────│─────────│─────────│──────────│
│   0    │ 0.12    │ 0.05    │ 0.25    │ 0.08    │ 0.35    │ 100     │ ▁▂▃▂▁▂▃ │
│   1    │ 0.15    │ 0.08    │ 0.30    │ 0.09    │ 0.42    │ 100     │ ▂▃▂▁▂▄▃ │
│  ...   │  ...    │  ...    │  ...    │  ...    │  ...    │  ...    │ ...      │
└───────────────────────────────────────────────────────────────────────────────┘

┌─ Throughput (MB/s) ──────────────────────────────────────────────────────────┐
│ Bucket │ Current │ Avg     │ Peak    │ Total Bytes     │ Sparkline          │
│────────│─────────│─────────│─────────│─────────────────│────────────────────│
│   0    │ 0.12    │ 0.11    │ 0.15    │ 45.2 MB         │ ▂▃▄▃▂▃▄▅▄▃▂▃▄     │
│   1    │ 0.10    │ 0.09    │ 0.13    │ 40.1 MB         │ ▂▃▂▃▄▃▂▃▄▃▂▃▂     │
│  ...   │  ...    │  ...    │  ...    │ ...             │ ...                │
│ TOTAL  │ 2.30    │ 2.20    │ 2.50    │ 900.5 MB        │ ▃▄▅▅▄▅▅▆▅▄▅▅▅     │
└───────────────────────────────────────────────────────────────────────────────┘

┌─ Protocol Health ────────────────────────────────────────────────────────────┐
│ Metric              │ Value      │ Status │ Threshold                       │
│─────────────────────│────────────│────────│─────────────────────────────────│
│ HB Loss Rate        │ 0.02%      │ ✅ OK  │ < 1%                            │
│ Resend Rate         │ 0.01%      │ ✅ OK  │ < 0.5%                          │
│ GAP Rate            │ 0.2/s      │ ✅ OK  │ < 10/s                          │
│ Avg Jitter          │ 0.13 ms    │ ✅ OK  │ < 1 ms                          │
│ Max Jitter          │ 0.42 ms    │ ⚠ WARN │ < 0.5 ms                       │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 5.3 交互控制

| 按键 | 功能 |
|------|------|
| F1 | 切换到 Network 视图 |
| F2 | 切换到 Topics 视图 |
| F3 | 切换到 Variables 视图 |
| F4 | 切换到 Performance 视图 |
| Q | 退出 |
| R | 重置所有统计 |
| P | 暂停/恢复刷新 |
| ↑/↓ | 滚动当前视图 |
| Tab | 切换焦点区域 |

---

## 6. 指标计算引擎 (metrics.py)

### 6.1 滑动窗口

```python
class SlidingWindow:
    """时间窗口滑动统计"""
    def __init__(self, window_sec=1.0):
        self.window_sec = window_sec
        self.events = []  # [(timestamp, value), ...]

    def add(self, ts: float, value: float = 1.0):
        self.events.append((ts, value))
        self._evict(ts)

    def _evict(self, now: float):
        cutoff = now - self.window_sec
        while self.events and self.events[0][0] < cutoff:
            self.events.pop(0)

    def rate(self) -> float:
        """返回每秒速率"""
        if not self.events:
            return 0.0
        duration = self.events[-1][0] - self.events[0][0]
        if duration <= 0:
            return 0.0
        return len(self.events) / duration

    def sum(self) -> float:
        return sum(v for _, v in self.events)

    def mean(self) -> float:
        if not self.events:
            return 0.0
        return sum(v for _, v in self.events) / len(self.events)
```

### 6.2 统计快照

```python
@dataclass
class StatsSnapshot:
    """某一时刻的全部统计快照，供 TUI 渲染"""
    timestamp: float
    uptime: float

    # Layer 1
    pkt_count: int
    pkt_rate: float
    byte_count: int
    throughput: float
    hb_count: int
    ack_count: int
    gap_count: int
    data_count: int
    hb_loss_rate: float
    resend_rate: float
    topic_stats: dict  # topic -> {pkts, bytes, rate}

    # Layer 2
    pub_nodes: list
    sub_nodes: list
    var_count: int
    bucket_var_count: list  # [20]
    sub_freq_dist: dict  # freq_ms -> count
    data_msg_rate: list  # [20]
    jitter_ms: list  # [20]
    throughput_per_bucket: list  # [20]
    mask_var_count_avg: list  # [20]
```

---

## 7. 配置 (config.py)

```python
@dataclass
class MonitorConfig:
    # 网络
    interface: str = "eth0"         # 抓包网卡
    bpf_filter: str = "udp and (port 7400 or port 7401)"

    # DDS
    domain_id: int = 66             # DDS domain

    # 刷新
    refresh_hz: int = 2             # TUI 刷新频率 (Hz)

    # 阈值
    hb_loss_warn: float = 0.01      # 1%
    hb_loss_crit: float = 0.05      # 5%
    resend_warn: float = 0.005      # 0.5%
    resend_crit: float = 0.02       # 2%
    jitter_warn_ms: float = 1.0     # 1ms
    jitter_crit_ms: float = 5.0     # 5ms

    # 滑动窗口
    rate_window_sec: float = 1.0
    jitter_window_size: int = 100
```

---

## 8. 依赖

```
# requirements.txt
scapy>=2.5.0
fastdds>=2.12.0
rich>=13.0
textual>=0.40
numpy>=1.24
```

---

## 9. 实施计划

| 阶段 | 内容 | 交付物 |
|------|------|--------|
| Phase 1 | 基础框架 | config.py + main.py + CLI 解析 |
| Phase 2 | RTPS 解析器 | rtps_parser.py (子消息解析 + 测试) |
| Phase 3 | 抓包层 | layer1_pcap.py (scapy 抓包 + 统计) |
| Phase 4 | DDS 层 | layer2_dds.py (fastdds 订阅 + 指标) |
| Phase 5 | 指标引擎 | metrics.py (滑动窗口 + 快照) |
| Phase 6 | TUI | layer3_tui.py (四视图交互终端) |
| Phase 7 | 集成测试 | 联调验证 |

---

## 10. 验证方式

1. 启动 `./build/demo_exec` (pub + sub 在同一进程)
2. 运行 `python tools/monitor/main.py --interface lo --domain 66`
3. 逐视图验证：
   - **F1**: 看到 RTPS 包统计、topic 分布、HB/ACK/GAP 计数
   - **F2**: 看到 tableDefine 和 subTableRegister 消息历史
   - **F3**: 看到变量列表、pub/sub 节点、bucket 分布
   - **F4**: 看到 jitter 时序图、吞吐量、协议健康状态
4. 交互验证：Q 退出、R 重置、P 暂停、↑↓ 滚动
