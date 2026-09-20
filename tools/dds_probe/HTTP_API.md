# onDemand Monitor HTTP API

监控工具自带一个轻量的 HTTP + JSON 接口，把终端界面（TUI）看到的同一份实时快照以 JSON 暴露出来，方便外部客户端（脚本、Web 前端、自动化监控）对接。

基于 `cpp-httplib` + `nlohmann-json`，实现位于 `http_server.cc` / `http_server.h`。

---

## 1. 如何启用

默认**不开启**。启动时用 `-p/--http-port` 指定监听端口即可：

```bash
# 在线抓包 + 开启 HTTP，监听 8080
sudo ./dds_probe -i eth0 -p 8080

# 离线回放 pcap + 开启 HTTP
./dds_probe -r capture.pcap -p 8080

# 仅离线诊断（headless dump 模式下不开启 HTTP）
./dds_probe -r capture.pcap -p 8080   # dump 模式不会启 HTTP
```

启动成功的标志：

```text
HTTP JSON endpoint on :8080
```

监听地址固定为 `0.0.0.0`，即局域网内其他机器也能访问（注意防火墙）。

> 注意：`-d/--dump`（headless 离线诊断）模式下 HTTP 不会启动，即使传了 `-p`。

完整用法：

```text
-i, --interface <name>    网卡（默认 any）
-r, --read <file>         离线读取 pcap 文件
-f, --filter <expr>       BPF 过滤表达式（默认 udp）
-p, --http-port <port>    HTTP JSON 端口（默认关闭）
-d, --dump                headless 离线诊断（此时不开 HTTP）
-h, --help                帮助
```

---

## 2. 通用约定

- **Base URL**：`http://<host>:<port>/`
- 全部是 **GET** 请求，无鉴权，无状态。
- 响应体为 JSON，格式化缩进 2 空格，`Content-Type: application/json`。
- 出错时：非 200 状态码 + `{"error": "..."}` 结构。
- 时间相关字段单位为**微秒**（`*_us`）。
- 所有字段在代码里都有，具体含义见下文各端点。

快速自检：

```bash
curl -s http://localhost:8080/api/full | head -50
```

---

## 3. 端点总览

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | 端点说明（纯文本） |
| GET | `/api/summary` | 全局汇总计数 |
| GET | `/api/participants` | 所有参与者（列表层） |
| GET | `/api/participants/<guid>` | 单个参与者 + 它发布的主题 |
| GET | `/api/topics` | 全局限性主题聚合 |
| GET | `/api/match` | 发布/订阅匹配关系 |
| GET | `/api/capture` | 抓包与队列统计 |
| GET | `/api/full` | 上面所有内容，一次 GET 拿全 |

> **推荐**：外部对接优先用 `/api/full`，一次请求拿到完整快照，避免多次请求造成的不一致。

---

## 4. 端点详情

### 4.1 `GET /` — 端点说明

纯文本说明，列出所有 API 路径。

```text
onDemand Monitor HTTP endpoint. See /api/participants, /api/topics, /api/match, /api/summary, /api/capture.
```

---

### 4.2 `GET /api/summary` — 全局汇总

```json
{
  "version": "1.0",
  "total_participants": 4,
  "total_endpoints": 12,
  "total_data_messages": 5231,
  "total_bytes": 10485760,
  "total_heartbeats": 98,
  "total_acknacks": 45,
  "total_nacks": 3
}
```

| 字段 | 类型 | 含义 |
|------|------|------|
| `version` | string | 接口版本 |
| `total_participants` | int | 已发现的参与者数量 |
| `total_endpoints` | int | 端点（publisher/subscriber 实体）总数 |
| `total_data_messages` | int | 数据消息总条数 |
| `total_bytes` | int | 累计字节数 |
| `total_heartbeats` | int | 心跳消息总数 |
| `total_acknacks` | int | ACKNACK 消息总数 |
| `total_nacks` | int | NACK 消息总数 |

---

### 4.3 `GET /api/participants` — 参与者列表

```json
{
  "participants": [
    {
      "guid": "0103c000000000000000000000000000",
      "name": "ondemand-pub",
      "domain_id": 0,
      "src_ip": "192.168.1.10",
      "multicast_ip": "239.255.0.1",
      "endpoints": 3,
      "active": true,
      "first_seen_us": 1700000000000000,
      "last_seen_us": 1700000001000000,
      "heartbeats": 20,
      "acknacks": 10,
      "nacks": 2
    }
  ]
}
```

| 字段 | 类型 | 含义 |
|------|------|------|
| `guid` | string | 参与者 GUID，32 位十六进制小写（16 字节：前缀+实体ID） |
| `name` | string | 参与者名称 |
| `domain_id` | int | DDS 域 ID |
| `src_ip` | string | 源 IP |
| `multicast_ip` | string | 组播 IP（可能为空） |
| `endpoints` | int | 端点数量 |
| `active` | bool | 是否活跃 |
| `first_seen_us` | int | 首次发现时间（微秒） |
| `last_seen_us` | int | 最近出现时间（微秒） |
| `heartbeats` | int | 心跳计数 |
| `acknacks` | int | ACKNACK 计数 |
| `nacks` | int | NACK 计数 |

---

### 4.4 `GET /api/participants/<guid>` — 单个参与者详情

`<guid>` 即参与者的 32 位十六进制 `guid`。

响应在 4.3 的基础上多了 `topics` 数组，列出该参与者发布/订阅的主题：

```json
{
  "guid": "0103c000000000000000000000000000",
  "name": "ondemand-pub",
  "domain_id": 0,
  "src_ip": "192.168.1.10",
  "multicast_ip": "239.255.0.1",
  "endpoints": 3,
  "active": true,
  "first_seen_us": 1700000000000000,
  "last_seen_us": 1700000001000000,
  "heartbeats": 20,
  "acknacks": 10,
  "nacks": 2,
  "topics": [
    {
      "topic_name": "var/data",
      "type_name": "dsf::var",
      "role": "W",
      "data_count": 1000,
      "bytes_sent": 2048000,
      "frag_count": 0,
      "ack_count": 10,
      "nack_count": 2,
      "unique_nacked_sn": 1,
      "fulfilled_nack_sn": 1,
      "unfulfilled_nack_sn": 0,
      "lost_count": 0,
      "loss_rate": 0.0,
      "retransmit_count": 0,
      "retransmit_rate": 0.0,
      "send_frequency_hz": 10.0
    }
  ]
}
```

未找到时返回 `404`：

```json
{
  "error": "participant not found"
}
```

**Topic 对象字段**（也用于 4.5）：

| 字段 | 类型 | 含义 |
|------|------|------|
| `topic_name` | string | 主题名 |
| `type_name` | string | 类型名 |
| `role` | string | `"W"` 发布 / `"R"` 订阅 / `"W+R"` 兼具 |
| `data_count` | int | 数据消息数 |
| `bytes_sent` | int | 发送字节数 |
| `frag_count` | int | 分片数 |
| `ack_count` | int | ACK 数 |
| `nack_count` | int | NACK 数 |
| `unique_nacked_sn` | int | 去重后的 NACK 序号数 |
| `fulfilled_nack_sn` | int | 已被满足的 NACK 序号数 |
| `unfulfilled_nack_sn` | int | 尚未满足的 NACK 序号数 |
| `lost_count` | int | 丢失条数 |
| `loss_rate` | float | 丢失率 |
| `retransmit_count` | int | 重传条数 |
| `retransmit_rate` | float | 重传率 |
| `send_frequency_hz` | float | 发送频率（Hz） |

---

### 4.5 `GET /api/topics` — 全局主题聚合

对所有参与者的主题做**去重并集**，并附上匹配信息（该主题是否有发布/订阅匹配上、各自的参与者）。除 4.4 的 Topic 字段外，额外增加：

| 字段 | 类型 | 含义 |
|------|------|------|
| `matched` | bool | 该主题的发布/订阅是否匹配上 |
| `writers` | array\<string\> | 该主题的发布方参与者 GUID 列表 |
| `readers` | array\<string\> | 该主题的订阅方参与者 GUID 列表 |

```json
{
  "topics": [
    {
      "topic_name": "var/data",
      "type_name": "dsf::var",
      "role": "W",
      "data_count": 1000,
      "bytes_sent": 2048000,
      "frag_count": 0,
      "ack_count": 10,
      "nack_count": 2,
      "unique_nacked_sn": 1,
      "fulfilled_nack_sn": 1,
      "unfulfilled_nack_sn": 0,
      "lost_count": 0,
      "loss_rate": 0.0,
      "retransmit_count": 0,
      "retransmit_rate": 0.0,
      "send_frequency_hz": 10.0,
      "matched": true,
      "writers": ["0103c000000000000000000000000000"],
      "readers": ["0103c000000000000000000000010000"]
    }
  ]
}
```

---

### 4.6 `GET /api/match` — 匹配关系

```json
{
  "matches": [
    {
      "topic_name": "var/data",
      "matched": true,
      "data_count": 1000,
      "writers": ["0103c000000000000000000000000000"],
      "readers": ["0103c000000000000000000000010000"]
    }
  ]
}
```

| 字段 | 类型 | 含义 |
|------|------|------|
| `topic_name` | string | 主题名 |
| `matched` | bool | 是否已匹配 |
| `data_count` | int | 该主题数据消息数 |
| `writers` | array\<string\> | 发布方 GUID 列表 |
| `readers` | array\<string\> | 订阅方 GUID 列表 |

---

### 4.7 `GET /api/capture` — 抓包与队列统计

```json
{
  "kernel_captured": 5231,
  "kernel_dropped": 0,
  "enqueued": 5231,
  "enqueue_dropped": 0,
  "malformed": 0,
  "queue_packets": 1523,
  "queue_bytes": 12480000,
  "iface_dropped": 0
}
```

| 字段 | 类型 | 含义 |
|------|------|------|
| `kernel_captured` | int | 内核层捕获的包数（`pcap_stats.ps_recv`） |
| `kernel_dropped` | int | 内核缓冲溢出丢包数（`ps_drop`） |
| `enqueued` | int | 成功入队数 |
| `enqueue_dropped` | int | 入队被拒（用户态队列满）丢包数 |
| `malformed` | int | 无法解析的畸形包数 |
| `queue_packets` | int | 当前队列积压包数 |
| `queue_bytes` | int | 当前队列积压字节数 |
| `iface_dropped` | int | 网卡丢包数（`ps_ifdrop`） |

> 调试高流量场景重点看 `kernel_dropped`（内核丢）和 `enqueue_dropped`（用户态队列丢）两个值。

---

### 4.8 `GET /api/full` — 全量快照（推荐）

一次性返回 `summary` + `participants`（含各自 `topics`）+ `matches` + `capture` 四大部分，结构与上面各端点一致，合并为一个对象：

```json
{
  "version": "1.0",
  "summary": {
    "total_participants": 4,
    "total_endpoints": 12,
    "total_data_messages": 5231,
    "total_bytes": 10485760,
    "total_heartbeats": 98,
    "total_acknacks": 45,
    "total_nacks": 3
  },
  "participants": [
    {
      "guid": "...",
      "name": "...",
      "topics": [ /* 见 4.4 */ ]
    }
  ],
  "matches": [ /* 见 4.6 */ ],
  "capture": { /* 见 4.7 */ }
}
```

---

## 5. 错误处理

- 未知参与者 → `404` + `{"error": "participant not found"}`
- 内部异常 → `500` + `{"error": "internal error"}`
- 请求了不存在的路径 → cpp-httplib 默认的 `404`

---

## 6. 调试与对接建议

**验证服务已启动：**

```bash
curl -s http://localhost:8080/
curl -s http://localhost:8080/api/summary
```

**看某个参与者详情**（GUID 从 `/api/participants` 拿）：

```bash
GUID=$(curl -s http://localhost:8080/api/participants | python3 -c "import sys,json;print(json.load(sys.stdin)['participants'][0]['guid'])")
curl -s http://localhost:8080/api/participants/$GUID
```

**轮询快照写日志：**

```bash
while true; do
  curl -s http://localhost:8080/api/capture >> capture.log
  echo >> capture.log
  sleep 5
done
```

**用 jq 提取关键指标：**

```bash
curl -s http://localhost:8080/api/capture | jq '.kernel_dropped, .enqueue_dropped, .queue_packets'
```

注意事项：

- 服务绑定 `0.0.0.0`，同一网段机器可直接访问；跨机器记得放行防火墙端口。
- 无鉴权、明文 HTTP，生产/公网环境请自行加反向代理或 ACL。
- 字段可能随实现演进，对接时以代码 `http_server.cc` 为准。
