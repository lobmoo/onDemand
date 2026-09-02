# 数据统计准确性修复

## 修复的问题

### 1. NACK vs 重传率 (retransmit_rate)

**问题现象：** 有NACK但retransmit=0，retransmit_rate=0%

**根本原因：**
- `nack_count` 统计的是 **ACKNACK submessage的数量**（reader请求重传）
- `retransmit_count` 统计的是 **实际重传的DATA包数量**（writer响应重传请求）
- 两者是异步的：reader发NACK → writer收到 → writer重传DATA

**正常情况：**
- NACK > 0 但 retransmit = 0：writer还没来得及重传，或者monitor没抓到重传包
- NACK ≈ retransmit：理想情况，每个NACK都对应一次重传
- retransmit > NACK：可能是timeout重传（非NACK触发）

**retransmit_rate 计算公式：**
```cpp
retransmit_rate = retransmit_count / data_count
```
表示：重传包占总数据包的比例

**不是bug，是正常的DDS RELIABLE协议行为**

---

### 2. Fragment（frag_count）含义

**是分片包：** `frag_count` 统计的是 **DATA_FRAG submessage的数量**

**场景：**
- 当数据包超过UDP MTU（通常1472字节）时，DDS会分片为多个DATA_FRAG
- 例如：64KB的TableDataTransfer会被分成 ~45个fragment

**修复前问题：**
- Fragment到达时，**只更新writer endpoint**，不更新reader endpoint
- 导致sub端的bucket topic显示 `Byte: 0`（实际在收数据）

**修复：** OnFragment现在同时更新reader endpoint的：
- `frag_count`
- `bytes_sent`
- `data_count`
- 调用`TrackRetransmitAndGaps(reader_ep, ...)`跟踪丢包/重传

---

### 3. Pub端datatransfer显示read属性

**问题：** Publisher节点的topic显示 `R` 标志（有reader），实际pub只写不读

**根本原因：**
- 旧代码为所有topic创建 **virtual reader**（synthetic GUID: `0000XX05`）
- 目的是支持multicast场景（ENTITYID_UNKNOWN），但在pub端创建是错的

**修复：** OnData现在只在 **subscriber节点** 创建virtual reader
```cpp
GUID_t writer_participant = GetParticipantGuid(data.writer_guid);
auto pubsub_it = discovered_pub_sub_.find(writer_participant);
bool is_sub_participant = (pubsub_it != discovered_pub_sub_.end() && !pubsub_it->second);

if (is_sub_participant) {
    // 只在sub端创建virtual reader
}
```

**判断依据：** 
- `discovered_pub_sub_[guid] = true/false`（true=pub, false=sub）
- 从participant name判断：`"Pub"` vs `"Sub"`

---

### 4. Sub端datatransfer的Byte没增加

**已修复：** 见问题2，Fragment现在会更新reader endpoint的bytes_sent

---

### 5. UI首页Topic Preview显示太多transfer

**不改UI的解决方案：** 从数据层解决
- Pub端不再创建virtual reader → Pub端topic不显示 `R` 标志
- Sub端的bucket topic现在正确显示字节数（Fragment修复）

**UI层可以进一步优化（可选）：**
- 首页只显示前5个topic，点进去再显示全部
- 或者过滤掉 `bucket_` 开头的topic，只在详情页显示

---

## 关键指标含义

| 指标 | 含义 | 计算方式 |
|------|------|---------|
| `data_count` | 数据包数量（包含重传） | 每个DATA/DATA_FRAG计数 |
| `frag_count` | 分片数量 | 每个DATA_FRAG计数 |
| `nack_count` | NACK请求数 | 每个ACKNACK submessage计数 |
| `retransmit_count` | 实际重传包数 | DATA包��kDupGuardUs内重复SN |
| `lost_count` | 丢包数 | 序列号gap检测 |
| `loss_rate` | 丢包率 | `lost / (data + lost)` |
| `retransmit_rate` | 重传率 | `retransmit / data` |

---

## 验证方法

**1. 验证Fragment统计（sub端）：**
```bash
# 启动1w+点的demo（会产生大量DATA_FRAG）
./build/demo_exec 10000 100

# 启动monitor
./build/tools/ondemand_monitor/ondemand_monitor

# 进入Sub节点 → 查看bucket_X topic
# 应该看到：
# - Byte: 增长中（之前是0）
# - frag_count > 0
# - data_count > 0
```

**2. 验证Pub端无Reader：**
```bash
# 进入Pub节点 → Topic Preview
# tableDefine, register 等topic应该只显示 W（writer），无R标志
```

**3. 验证NACK vs Retransmit：**
```bash
# 模拟丢包场景（需要网络抖动或高负载）
# 如果看到 NACK>0 但 retrans=0，不是bug：
# - 可能writer还没重传
# - 可能monitor没抓到重传包（BPF过滤器问题）
# - 如果持续30秒以上retrans还是0，才可能有问题
```

---

## 代码修改位置

**metrics_engine.cc:**
1. `OnFragment()` L1041-1067: 增加reader endpoint统计
2. `OnData()` L651-695: Virtual reader只在sub端创建

**测试命令：**
```bash
cd build
make -j$(nproc)
./tools/ondemand_monitor/ondemand_monitor
```
