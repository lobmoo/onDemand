# OnDemand 核心问题与解决方案

> **版本**: v1.0 | **最后更新**: 2026-07-15 | **作者**: wwk

本文档记录 OnDemand 项目开发过程中遇到的关键技术难题及其解决方案。

---

## 1. 大规模变量表广播 — 分表机制

### 问题

工业场景中单个发布节点可能管理 **10 万+ 变量**。将所有变量定义放在一条 DDS 消息中广播：

- 单条消息体积过大（10 万 × 每条 Define ~100B ≈ 10MB），超出 DDS 单帧限制
- 序列化/反序列化耗时长，阻塞 DDS 回调线程
- 任意一个变量变更都需要重发全量定义，带宽浪费严重

### 解决方案：按 Bucket 分表广播

将变量按哈希取模分配到 **20 个桶（Bucket）**，每个桶对应一张独立的 `PubTableDefine`，通过独立的 DDS Topic 广播。

```
变量注册流程:
  varHash = FNV-1a("nodeName_varName")
  bucketId = varHash % 20
  → 存入 BucketManager.buckets_[bucketId]

广播流程 (createVars):
  for each affectedBucket:
      PubTableDefine.name = "bucket_N"
      PubTableDefine.varDefines = [该桶内所有变量的 Define]
      → tableDefinePublish()  // 按桶分批发送
```

**关键代码路径**:
- `BucketManager::CalculateBucketIndexFromHash()` — [on_demand_common.h:78](core/ondemand/on_demand_common.h#L78)
- `createVars()` 按 `affectedBuckets` 分批广播 — [on_demand_pub.cc:789-843](core/ondemand/on_demand_pub.cc#L789-L843)

**效果**:
- 单条消息从 ~10MB 降至 ~500KB（10万/20 = 5000条/桶 × 100B）
- 变量增删时只重发受影响的桶，增量更新
- 20 个桶天然并行，不阻塞

---

## 2. 变量标识内存优化 — Hash 编码

### 问题

10 万+ 变量，如果用字符串作为索引 key：

- 每个变量名 `nodeName_varName` 平均 ~50 字节
- `std::unordered_map<std::string, VarMetadata>` 中每个 entry 的 string 开销 = 50B + 32B (SSO/堆) = ~80B
- 10 万变量仅 key 就占 ~8MB，加上哈希表本身的 bucket 开销更大
- 热路径（`setVarData`）每次写入都要做字符串哈希查找

### 解决方案：FNV-1a 哈希编码

所有变量标识统一使用 **FNV-1a 64 位哈希**，将字符串压缩为 8 字节整数：

```cpp
// on_demand_common.h
inline uint64_t fast_hash(const char *str) {
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    while (*str) {
        hash ^= static_cast<uint64_t>(*str++);
        hash *= 0x100000001b3ULL;           // FNV prime
    }
    return hash;
}

// 变量名格式: "nodeName_varName" → 全局唯一
std::string varName = make_meta_varname(nodeName, varDefine.name());
uint64_t varHash = fast_hash(varName);
```

**全链路 Hash 化**:

| 环节 | 字符串方案 | Hash 方案 |
|------|-----------|----------|
| 索引 key | `string` (~50B) | `uint64_t` (8B) |
| `VarStore` 内部表 | `unordered_map<string, id>` | `unordered_map<uint64_t, id>` |
| DDS 传输 mask | 字符串列表 | `Roaring64Map` 压缩位图 |
| 比较开销 | 字符串逐字节比较 | 整数一次比较 |
| 内存占用 (10万变量) | ~8MB (key only) | ~800KB (key only) |

**关键代码路径**:
- `fast_hash()` — [on_demand_common.h:193-203](core/ondemand/on_demand_common.h#L193-L203)
- `make_meta_varname()` — [on_demand_common.h:211](core/ondemand/on_demand_common.h#L211)
- `varIndex_` 使用 `uint64_t` 作为 key — [on_demand_pub.h:330](core/ondemand/on_demand_pub.h#L330)

**注意**: FNV-1a 理论上存在哈希冲突，但 64 位空间下 10 万变量的冲突概率极低（< 10⁻¹⁰），工程上可忽略。

---

## 3. 多 Pub/Sub 通信干扰 — 动态 Partition 机制

### 问题

DDS 默认同一 Domain 内所有 Subscriber 都能收到所有 Publisher 的数据。在多 Pub/Sub 场景下：

```
PubA (节点A) ──发布变量──→ DDS ──→ SubX, SubY, SubZ
PubB (节点B) ──发布变量──→ DDS ──→ SubX, SubY, SubZ

问题: SubX 只订阅了 PubA 的变量，但仍会收到 PubB 的数据
     → 应用层被迫处理无关数据，浪费 CPU 和带宽
```

### 解决方案：DDS Partition 按节点名动态隔离

利用 DDS 的 **Partition（分区）** QoS 机制，将数据传输 Topic 按发布者节点名隔离：

```cpp
// Pub 端 init(): 创建 Publisher 时设置 partition = "dsf/var/data/transfer/nodeName"
DdsWrapper::PublisherQoSBuilder pubQos;
pubQos.setPartition("dsf/var/data/transfer/" + nodeName_);
dataNode_->createPublisher(DATA_TRANSFER_PUB_SUB_NAME, pubQos);

// Sub 端 subscribe(): 订阅时动态打开对应 partition
bool OnDemandSub::setPartition(string name, string partitionName) {
    std::lock_guard<std::mutex> lk(activePartitionsMutex_);
    activePartitions_.insert(partitionName);  // 累加，不覆盖
    for (const auto &p : activePartitions_) {
        subQos.setPartition(p);  // 重新设置全量 partition 列表
    }
    dataNode_->updateSubscriberQos(name, subQos);
}

// subscribe() 调用时自动打开对应 partition
setPartition(DATA_TRANSFER_PUB_SUB_NAME,
             "dsf/var/data/transfer/" + node_name);
```

**通信隔离效果**:

```
PubA (partition: "dsf/var/data/transfer/A") ──→ DDS
PubB (partition: "dsf/var/data/transfer/B") ──→ DDS

SubX 订阅了 PubA 的变量:
  activePartitions_ = {"dsf/var/data/transfer/A"}
  → 只收到 PubA 的数据，PubB 的数据被 DDS 层过滤

SubY 订阅了 PubA + PubB 的变量:
  activePartitions_ = {"dsf/var/data/transfer/A", "dsf/var/data/transfer/B"}
  → 收到两者的数据
```

**关键代码路径**:
- Pub 端 partition 设置 — [on_demand_pub.cc:72-74](core/ondemand/on_demand_pub.cc#L72-L74)
- Sub 端 `setPartition()` — [on_demand_sub.cc:572-589](core/ondemand/on_demand_sub.cc#L572-L589)
- `subscribe()` 中调用 `setPartition()` — [on_demand_sub.cc:693-694](core/ondemand/on_demand_sub.cc#L693-L694)

**效果**:
- DDS 层面过滤，不消耗应用层 CPU
- 动态累积 partition，支持多次订阅不同节点
- 带宽节省：N 个 Pub 节点时，Sub 只接收 1/N 的流量

---

## 4. 断网重连时序竞争 — GUID 区分新旧实例

### 问题

DDS 的 Participant 发现机制存在**时序竞争**：当 Pub 节点掉线后快速重启时，Sub 可能收到以下乱序事件：

```
时间线:
  T1: Pub 旧实例掉线
  T2: Pub 新实例启动，DDS 发现 → DISCOVERED 事件到达 Sub
  T3: Pub 旧实例的 DROPPED 事件到达 Sub  ← 延迟到达！

问题: 如果 Sub 在 T3 收到 DROPPED 时清理了变量，
     但 T2 已经收到了新实例的定义 → 新实例的变量被误清！
```

### 解决方案：GUID 追踪 + Dropped 集合

每个 DDS Participant 都有全局唯一的 **GUID**。Sub 端维护两层映射：

```
pubGuids_:         pubName → currentGuid    (当前活跃实例的 GUID)
droppedPubGuids_:  Set<guid>                (被 DROPPED 过的 GUID 集合)
```

**核心逻辑**:

```cpp
void OnDemandSub::onParticipantDiscovery(const ParticipantInfo &info) {

    if (info.status == DISCOVERED) {
        // 检测重连: 如果这个 GUID 之前被 DROPPED 过，说明是同一实例重连
        isReconnect = droppedPubGuids_.count(info.guid) > 0;
        if (isReconnect) {
            droppedPubGuids_.erase(info.guid);
            // 恢复变量定义缓存 + 重发订阅请求
            restorePubTableDefineFromCache(info.participant_name);
            resendSubscriptions();
        }
        pubGuids_[info.participant_name] = info.guid;  // 更新当前 GUID
        return;
    }

    if (info.status == DROPPED) {
        // 关键: 检查 DROPPED 的 GUID 是否是当前实例
        auto it = pubGuids_.find(info.participant_name);
        if (it != pubGuids_.end() && it->second != info.guid) {
            // GUID 不匹配 → 旧实例的延迟 DROPPED，忽略！
            return;
        }
        // GUID 匹配 → 记录到 dropped 集合，正常清理
        droppedPubGuids_.insert(it->second);
        pubGuids_.erase(info.participant_name);
        cleanupParticipantPublish(info.participant_name);
    }
}
```

**时序图**:

```
场景: PubA 掉线后快速重启，旧 DROPPED 延迟到达

  Sub                              DDS
   │                                │
   │  ← DISCOVERED (PubA, guid=G2)  │  T2: 新实例上线
   │  pubGuids_["PubA"] = G2        │
   │                                │
   │  ← DROPPED (PubA, guid=G1)     │  T3: 旧实例的延迟通知
   │  pubGuids_["PubA"] = G2 ≠ G1   │
   │  → 忽略！不清除变量             │
   │                                │
   │  ← DISCOVERED (PubA, guid=G2)  │  如果 G2 之前进过 droppedPubGuids_
   │  isReconnect = true             │  → 恢复缓存 + 重发订阅
```

**关键代码路径**:
- Sub 端 `onParticipantDiscovery()` — [on_demand_sub.cc:1105-1157](core/ondemand/on_demand_sub.cc#L1105-L1157)
- Pub 端 `onParticipantDiscovery()` — [on_demand_pub.cc:1519-1544](core/ondemand/on_demand_pub.cc#L1519-L1544)
- `droppedPubGuids_` 集合 — [on_demand_sub.h:376](core/ondemand/on_demand_sub.h#L376)
- `pubTableDefineCache_` 缓存 — [on_demand_sub.h:380-383](core/ondemand/on_demand_sub.h#L380-L383)

---

## 5. 同批次同频率分批回调 — (Bucket, Freq) 分组调度

### 问题

10 万变量按不同频率回调用户，如果每个变量独立一个定时器：

- 10 万定时器 → 时间轮爆炸，调度开销巨大
- 同一频率的变量多次触发回调，用户被迫逐个处理
- 回调时间点不一致，用户无法做批量数据快照

### 解决方案：(bucketIndex, freqMs) 二维分组

将变量按 **(桶索引, 频率)** 二维分组，同组变量共享一个定时器，一次触发批量回调：

```
分组维度: PublishGroupKey = { bucketIndex, freqMs }

示例 (bucket=3):
  变量A (10ms), 变量B (10ms), 变量C (10ms)  → Group(3, 10ms)  → 一个定时器
  变量D (100ms), 变量E (100ms)               → Group(3, 100ms) → 一个定时器
  变量F (1000ms)                             → Group(3, 1000ms)→ 一个定时器

定时器触发时:
  Group(3, 10ms) → 批量读取 A,B,C → 一次回调 callback([A,B,C])
```

**Pub 端调度**:

```cpp
// processPublishTaskScheduler(): 构建期望分组
for (const auto &[varHash, meta] : varIndex_) {
    if (meta.currentFreq == 0xFFFFFFFF) continue;  // 无订阅者
    PublishGroupKey key{meta.bucketIndex, meta.currentFreq};
    desired[key].push_back(GroupVarInfo{varHash, meta.varId, runtimeSize});
}

// 增量 diff: 只新增/移除变化的定时器
for (auto &[key, members] : desired) {
    if (publishGroupTimers_.find(key) == publishGroupTimers_.end()) {
        // 新分组 → 创建 ScheduleRecurring
        // 按 bucket 错开初始延迟，避免同频同桶同时触发
        Tick jitter = bucketIdx * staggerStep;
        timer = scheduler->ScheduleRecurring(publishGroupData, interval + jitter, interval);
    }
}
```

**Sub 端回调调度**:

```cpp
// processCallbackScheduler(): 同样按 (bucket, freq) 分组
CallbackGroupKey key{lu.bucketIndex, cbInfo.freqMs};
desired[key].members.push_back(CallbackVarInfo{...});

// 定时器触发 callbackGroupData()
// → 批量零拷贝读取 → 构造 vector<VarCallbackData> → 一次用户回调
// 定时器比 Pub 端晚 5ms 触发，确保 Pub 已写完
Tick jitter = bucketIdx * staggerStep + 5;  // +5ms 延迟
```

**关键代码路径**:
- Pub 端 `processPublishTaskScheduler()` — [on_demand_pub.cc:397-514](core/ondemand/on_demand_pub.cc#L397-L514)
- Pub 端 `publishGroupData()` — [on_demand_pub.cc:527-645](core/ondemand/on_demand_pub.cc#L527-L645)
- Sub 端 `processCallbackScheduler()` — [on_demand_sub.cc:788-936](core/ondemand/on_demand_sub.cc#L788-L936)
- Sub 端 `callbackGroupData()` — [on_demand_sub.cc:943-1047](core/ondemand/on_demand_sub.cc#L943-L1047)

**效果**:
- 定时器数量: 从 10 万 降至 ~400（20 bucket × 20 种常见频率）
- 回调次数: 从 10 万次/秒 降至 ~400 次/秒
- 用户回调天然获得同组变量的批量快照

---

## 6. PubTableDefine 持久化 — @key + History 避免重复广播

### 问题

`PubTableDefine` 是 DDS 的 TRANSIENT_LOCAL 话题（持久化）。如果每次 `createVars()` 都全量广播：

- 新 Sub 上线时，DDS 会重发所有历史消息 → 大量重复数据
- 同一桶的变量定义被多次覆盖，Sub 端反复解析处理

### 解决方案：利用 IDL @key 字段 + History QoS

在 IDL 定义中，`PubTableDefine` 使用 **`@key` 联合主键**：

```idl
// dsf_define_var.idl
struct PubTableDefine {
    @key string name;      // 表名称（如 "bucket_3"）
    @key string nodeName;  // 所属节点名称
    string description;
    sequence<PubTableVarDefine> varDefines;
};
```

配合 DDS QoS 配置：

```cpp
// Pub 端 createTableDefineWriter()
writerQosBuilder
    .setDurabilityKind(TRANSIENT_LOCAL)    // 持久化：新 Sub 上线时收到最后一条
    .setReliabilityKind(RELIABLE)          // 可靠传输
    .setHistoryKind(KEEP_LAST)             // 只保留最后一条
    .setHistoryDepth(1);                   // depth=1
```

**效果**:

```
Pub 端多次广播同一 bucket:
  createVars([A, B, C]) → 发送 PubTableDefine{name="bucket_3", vars=[A,B,C]}
  createVars([D, E])    → 发送 PubTableDefine{name="bucket_3", vars=[A,B,C,D,E]}
                          ↑ @key 相同 (name + nodeName)，DDS 替换而非追加

新 Sub 上线:
  DDS 自动推送 PubTableDefine{name="bucket_3", vars=[A,B,C,D,E]}  ← 只有一条，不是两条
```

**关键代码路径**:
- IDL `@key` 定义 — [dsf_define_var.idl:123-128](core/ondemand/idl/dsf_define_var.idl#L123-L128)
- Writer QoS 配置 — [on_demand_pub.cc:139-148](core/ondemand/on_demand_pub.cc#L139-L148)
- `tableDefinePublish()` — [on_demand_pub.cc:265-273](core/ondemand/on_demand_pub.cc#L265-L273)

---

## 7. 轻量化变量注册接口 — registerVars 懒创建

### 问题

`createVars()` 会完整执行：分配 VarStore 内存 + 注册 BucketManager + 广播 PubTableDefine。对于 10 万+ 变量的场景：

- 大部分变量可能永远不会被订阅（如调试变量、备选测点）
- 预分配 VarStore arena 浪费大量内存（每个 slot 即使 size=0 也有 64B header + 128B 双缓冲）
- 10 万变量 × 192B ≈ 18MB 纯 header 开销

### 解决方案：registerVars 轻量注册 + 懒创建

提供两级注册接口：

```cpp
// 级别1: registerVars() — 轻量注册，不分配内存
//   只记录 name + hash 到 liteBucketMembers_，广播最小 PubTableDefine
//   变量在首次被 subscribe 触发时才真正 createVars（懒创建）
bool registerVars(const std::vector<DSF::Var::Define> &VarDefines);

// 级别2: createVars() — 完整注册，分配内存
//   注册 VarStore + BucketManager + 广播完整 PubTableDefine
bool createVars(const std::vector<DSF::Var::Define> &VarDefines);
```

**轻量注册内部实现**:

```cpp
// registerVars(): 只提取 name + size，存入紧凑结构
struct LiteVarEntry {
    uint64_t hash;
    uint32_t nameOffset;  // 指向 namePool_ 的偏移，避免 string 独立堆分配
};
struct LiteBucket {
    std::vector<LiteVarEntry> entries;
    std::unordered_set<uint64_t> hashSet;  // O(1) 去重
};
std::array<LiteBucket, 20> liteBucketMembers_;
std::vector<char> namePool_;  // 所有变量名紧凑存储，一个连续 buffer

// 广播时构造最小 Define（仅 name + nodeName，足够 sub 注册）
DSF::Var::Define liteDefine;
liteDefine.name(nameStr);
liteDefine.nodeName(nodeName_);
```

**内存对比**:

| 方案 | 10 万变量内存占用 | 说明 |
|------|-----------------|------|
| `createVars()` 全量 | ~18MB (header) + arena | 每个变量 64B Slot header + 128B 双缓冲 |
| `registerVars()` 轻量 | ~2.4MB | 8B hash + 4B offset + namePool ≈ 24B/变量 |
| 节省 | **~85%** | 仅在被订阅时按需分配 |

**关键代码路径**:
- `registerVars()` — [on_demand_pub.cc:850-884](core/ondemand/on_demand_pub.cc#L850-L884)
- `broadcastLiteTableDefines()` — [on_demand_pub.cc:886-921](core/ondemand/on_demand_pub.cc#L886-L921)
- `LiteBucket` / `namePool_` 结构 — [on_demand_pub.h:339-349](core/ondemand/on_demand_pub.h#L339-L349)

---

## 8. 同频定时器爆发 — 发布相位错峰

### 问题

当大量变量集中在同一频率（如 10ms）时，20 个桶的定时器会在**同一 tick 瞬间全部触发**：

```
tick=100ms:
  Group(0, 10ms)  → 触发 → publishGroupData(0, 10)
  Group(1, 10ms)  → 触发 → publishGroupData(1, 10)
  Group(2, 10ms)  → 触发 → publishGroupData(2, 10)
  ...
  Group(19, 10ms) → 触发 → publishGroupData(19, 10)

问题: 20 个 publishGroupData 同时涌入 timer 线程
     → 单个 tick 上挂了 20 个 task
     → 执行不完，后续 tick 被延迟，频率精度失真
```

更严重的是，Sub 端的回调定时器如果也在同一时刻触发，20 个 `callbackGroupData` 同时执行，线程池被瞬间打满。

### 解决方案：按 Bucket 错开初始延迟（发布相位）

为每个桶的定时器添加一个**相位偏移（jitter）**，按 bucketIndex 线性错开：

```cpp
// 计算错峰步长: 频率 / 桶数，最小 5ms
Tick staggerStep = freqMs / ONDEMAND_BUCKET_SIZE;
if (staggerStep < 5) staggerStep = 5;  // 最小步长 5 ticks

// 每个桶的初始延迟 = 基础间隔 + 相位偏移
Tick jitter = bucketIndex * staggerStep;
auto timer = scheduler->ScheduleRecurring(
    publishGroupData, intervalTicks + jitter, intervalTicks);

// Sub 端同理，额外 +5ms 确保 Pub 已写完
Tick subJitter = bucketIndex * staggerStep + 5;
```

**错峰效果（以 10ms 频率为例）**:

```
错峰前（所有桶同时触发）:
  tick=10ms:  [bucket 0~19 全部触发] ← 爆发！

错峰后（按桶线性错开，步长 5ms）:
  tick=10ms:  [bucket 0]      触发 → 发送 → 完成
  tick=15ms:  [bucket 1]      触发 → 发送 → 完成
  tick=20ms:  [bucket 2]      触发 → 发送 → 完成
  ...
  tick=105ms: [bucket 19]     触发 → 发送 → 完成
  tick=110ms: [bucket 0]      下一轮...

效果: 每个 tick 最多 1 个 task，timer 线程轻松处理
```

**Sub 端额外 +5ms 的原因**:

```
Pub bucket=3 相位: tick=25ms  触发 → DDS write
Sub bucket=3 相位: tick=30ms  触发 → VarStore read → 回调

+5ms 间隙确保:
  1. Pub 的 DDS write 已完成
  2. Sub 的 VarStore 已收到最新数据
  3. 用户回调读到的是最新值
```

**为什么用线性错开而不是随机**:
- 线性错开保证任意两个桶之间间隔固定，负载均匀
- 随机抖动可能偶然让多个桶撞到同一 tick
- 线性错开可预测，便于性能分析和调优

---

## 9. 订阅先于变量定义到达 — 指数退避重试队列

### 问题

在分布式系统中，Sub 端的 `subscribe()` 请求可能**先于 Pub 端的 `createVars()` 到达**：

```
时间线:
  T1: Pub 启动，开始 createVars()... (耗时较长)
  T2: Sub 启动，subscribe("varA", 10ms) → SubTableRegister 发出
  T3: Pub 收到订阅请求，但 varA 尚未注册到 varIndex_ → 订阅失败！
  T4: Pub 的 createVars("varA") 完成 → 变量已注册
      但订阅请求已被丢弃 → varA 永远不会有订阅者！
```

传统方案直接丢弃未匹配的订阅请求，但在这个时序窗口下会导致**订阅永久丢失**。

### 解决方案：独立重试队列 + 指数退避

将正常消息队列与重试队列分离，未匹配的订阅请求不丢弃，进入重试队列持续重试：

```cpp
// 两个队列，互不阻塞
moodycamel::ConcurrentQueue<...> pubTableDefRegisterQueue_;   // 正常队列（新消息）
std::queue<...> pubTableDefRetryQueue_;                        // 重试队列（未匹配的）

// 消费逻辑: 优先正常队列，空了再消费重试队列
while (running) {
    data = normalQueue.try_dequeue() ?: retryQueue.pop();  // 优先新消息
    if (!data) { sleep(100ms); continue; }

    missing = handleSubscribe(nodeName, varFreqs);
    if (missing > 0) {
        // 有变量未找到 → 构造只含缺失变量的重试消息
        retryData = onlyMissingVars(data);

        // 指数退避: 200ms → 400ms → 800ms → 1600ms → 3000ms (cap)
        backoffMs = min(3000, backoffMs * 2);
        sleep(backoffMs);
        retryQueue.push(retryData);
    } else {
        // 全部匹配 → 清除该节点的退避计时
        backoffMs = 0;
    }
}
```

**退避策略细节**:

```
退避参数:
  部分缺失 (some missing):  起始 200ms,  最大 3000ms
  全部缺失 (all missing):   起始 800ms,  最大 3000ms

退避序列（部分缺失）:
  第1次重试: 200ms 后
  第2次重试: 400ms 后
  第3次重试: 800ms 后
  第4次重试: 1600ms 后
  第5次+重试: 3000ms 后 (封顶)

全部缺失时起始更高:
  第1次重试: 800ms 后  ← 变量完全未创建，等待更久更合理
  第2次重试: 1600ms 后
  第3次+重试: 3000ms 后 (封顶)
```

**为什么分两个队列**:

```
如果用单一队列:
  新消息和重试消息混在一起
  → 重试消息可能排在新消息前面，阻塞新请求处理
  → 新的 subscribe 请求被旧的重试请求堵住

双队列:
  正常队列 (ConcurrentQueue): DDS 回调线程无锁入队
  重试队列 (std::queue+mutex): 处理线程独占

  消费顺序: 正常队列优先 → 保证新请求不被重试阻塞
           正常队列空时 → 才消费重试队列
```

**取消订阅时清理重试队列**:

```cpp
// 处理 SUB_TABLE_UNREGISTER 时，清理该节点的所有待重试消息
case SUB_TABLE_UNREGISTER:
    // 1. 清除退避计时
    retryBackoffMsByNode.erase(unsubNode);

    // 2. 过滤重试队列，移除该节点的所有消息
    //    防止: unsubscribe 后重试队列中残留的旧订阅请求又被执行
    while (!retryQueue.empty()) {
        item = retryQueue.pop();
        if (item.nodeName != unsubNode)
            remaining.push(item);
    }
    retryQueue = remaining;

    // 3. 正常处理取消订阅
    handleUnsubscribe(unsubNode, varFreqs);
```

**Sub 端变量定义先到、订阅后到的反向场景**也覆盖：

```
T1: Sub 收到 PubTableDefine → 注册到 varIndex_
T2: 用户调用 subscribe() → SubTableRegister 发出
T3: Pub 收到 → handleSubscribe() → varIndex_ 中已存在 → 直接成功

这个方向没有问题，因为 varIndex_ 在 T1 就已经就绪。
重试队列解决的是 Pub 端变量未就绪的方向。
```

---

## 总结

| # | 问题 | 解决方案 | 核心收益 |
|---|------|---------|---------|
| 1 | 10 万变量表广播 | 按 hash%20 分桶分表 | 单消息 10MB → 500KB，增量更新 |
| 2 | 变量标识内存开销 | FNV-1a 64 位哈希编码 | key 80B → 8B，全链路整数化 |
| 3 | 多 Pub/Sub 通信干扰 | DDS Partition 按节点名隔离 | DDS 层过滤，带宽节省 (N-1)/N |
| 4 | 断网重连时序竞争 | GUID 追踪 + dropped 集合 | 区分新旧实例，防止误清变量 |
| 5 | 回调调度开销 | (bucket, freq) 二维分组 | 定时器 10 万 → 400，批量回调 |
| 6 | 重复广播 PubTableDefine | @key + TRANSIENT_LOCAL + KEEP_LAST | DDS 自动替换，新 Sub 只收一条 |
| 7 | 变量预分配内存浪费 | registerVars 懒创建 | 内存节省 ~85%，按需分配 |
| 8 | 同频定时器爆发 | 按 Bucket 线性错开相位 | 单 tick 最多 1 个 task，频率精度保障 |
| 9 | 订阅先于变量到达 | 独立重试队列 + 指数退避 | 订阅永不丢失，退避避免忙等 |
