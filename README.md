# OnDemand 模块架构文档

## 概述

OnDemand 是一个基于 FastDDS 的高性能按需变量发布/订阅系统，核心目标是：

- 支持海量变量（10万+）的高效分发
- 订阅方按需指定各变量的更新频率
- 发布方只以订阅方要求的最低频率推送数据，避免无效传输
- 支持运行时动态调整订阅关系和频率

典型场景：一个系统有大量状态变量，不同消费者对不同变量有不同的刷新率需求，OnDemand 让每个变量只以"最需要的频率"被推送。

---

## 项目结构

```
core/
├── ondemand/
│   ├── on_demand_pub.h/cc      # 发布端
│   ├── on_demand_sub.h/cc      # 订阅端
│   ├── on_demand_common.h      # 公共工具（BucketManager、VarMetadata 等）
│   ├── variable_store.h        # 变量数据存储
│   ├── concurrentqueue.h       # 无锁队列（moodycamel）
│   └── idl/                    # DDS 消息定义
│       ├── dsf_define_var.idl
│       ├── dsf_define_messages.idl
│       └── dsf_define_datatypes.idl
└── main.cc                     # 示例/测试代码

common/
├── timer_wheel/                # 时间轮调度器
│   ├── timer_wheel.h           # 分层时间轮核心
│   ├── timer_scheduler.h       # 调度器封装
│   └── thread_pool.h           # 线程池
├── fastdds_wrapper/            # FastDDS 封装层
└── log/                        # 日志
```

---

## 整体架构

```
┌──────────────────────────────────────┐       ┌──────────────────────────────────────┐
│            Publisher Node             │       │           Subscriber Node             │
│                                      │       │                                      │
│  ┌────────────────────────────────┐  │       │  ┌────────────────────────────────┐  │
│  │          OnDemandPub           │  │       │  │          OnDemandSub           │  │
│  │                                │  │       │  │                                │  │
│  │  varIndex_  (hash→VarMetadata) │  │       │  │  varIndex_  (hash→VarMetadata) │  │
│  │  varStore_  (变量数据内存池)    │  │       │  │  varStore_  (变量数据内存池)    │  │
│  │  bucketManager_ (20分桶)       │  │       │  │  varWriteStamps_ (写时间戳)     │  │
│  │  publishScheduler_ (时间轮)    │  │       │  │  callbackScheduler_ (时间轮)    │  │
│  │  publishGroupTimers_           │  │       │  │  callbackGroupTimers_           │  │
│  │  dataTransferWriterMap_        │  │       │  │  dataTransferReaderMap_         │  │
│  └────────────────────────────────┘  │       │  └────────────────────────────────┘  │
└──────────────────────────────────────┘       └──────────────────────────────────────┘
              │                                               │
              │                FastDDS (Domain 66)            │
              │   ┌──────────────────────────────────────┐   │
              └──►│  dsf/sys/var/tableDefine             │──►│  PubTableDefine（变量定义广播）
                  │  dsf/message/.../subTableRegister  ◄─│◄──│  SubTableRegister（订阅请求）
                  │  dsf/var/data/transfer/bucket_N    ──│──►│  TableDataTransfer（数据推送）
                  └──────────────────────────────────────┘
```

---

## 核心组件

### 1. OnDemandPub（发布端）

负责变量注册、数据更新、订阅处理和定时推送。

**关键成员变量：**

```cpp
// 变量索引：变量名 hash → 元数据
std::unordered_map<uint64_t, VarMetadata> varIndex_;
std::shared_mutex varIndexMutex_;

// 分桶管理（20个桶）
BucketManager bucketManager_;

// 变量数据存储
VarStore varStore_;

// 节点槽位映射：节点名 hash → bit 位置（最多64个节点）
std::unordered_map<uint64_t, uint8_t> nodeSlotMap_;
uint8_t nextNodeSlot_ = 0;

// DDS 通信
std::shared_ptr<DDSTopicWriter<PubTableDefine>>    pubTableDefineWriter_;
std::shared_ptr<DDSTopicReader<SubTableRegister>>  subTableRegisterReqReader_;
std::unordered_map<uint32_t,
    std::shared_ptr<DDSTopicWriter<TableDataTransfer>>> dataTransferWriterMap_;  // 每桶一个

// 发布调度
std::unique_ptr<TimerScheduler> publishScheduler_;
std::unordered_map<PublishGroupKey,
    std::shared_ptr<TimerEventInterface>> publishGroupTimers_;  // (桶,频率) → 定时器
std::unordered_map<PublishGroupKey,
    std::shared_ptr<std::vector<GroupVarInfo>>>   groupMembers_;  // (桶,频率) → 变量列表
std::atomic<bool> schedulerDirty_{true};

// 订阅请求队列（无锁）
moodycamel::ConcurrentQueue<std::shared_ptr<SubTableRegister>> pubTableDefRegisterQueue_;
```

**关键方法：**

```cpp
bool init(const std::string& nodeName);
bool start();
bool createVars(const std::vector<DSF::Var::Define>& varDefines);
bool setVarData(const char* varName, const void* data, size_t size);
bool deleteVars(const std::vector<std::string>& varNames);
void stop();
```

---

### 2. OnDemandSub（订阅端）

负责接收变量定义、发起订阅、接收数据并触发回调。

**关键成员变量：**

```cpp
// 变量索引
std::unordered_map<uint64_t, VarMetadata> varIndex_;
std::shared_mutex varIndexMutex_;

// 变量数据存储
VarStore varStore_;

// 写时间戳追踪（用于回调时检测变更）
struct VarWriteStamp {
    std::atomic<uint64_t> timestampNs{0};
    std::atomic<uint32_t> writeCount{0};
};
std::unique_ptr<VarWriteStamp[]> varWriteStamps_;

// 订阅回调注册表
std::unordered_map<uint64_t, SubCallbackInfo> subscriptionCallbacks_;
std::mutex subscriptionCallbacksMutex_;

// DDS 通信
std::shared_ptr<DDSTopicReader<PubTableDefine>>    pubTableDefineReader_;
std::shared_ptr<DDSTopicWriter<SubTableRegister>>  subTableRegisterReqWriter_;
std::unordered_map<uint32_t,
    std::shared_ptr<DDSTopicReader<TableDataTransfer>>> dataTransferReaderMap_;  // 每桶一个

// 回调调度
std::unique_ptr<TimerScheduler> callbackScheduler_;
std::unordered_map<CallbackGroupKey,
    std::shared_ptr<TimerEventInterface>>          callbackGroupTimers_;  // 频率 → 定时器
std::unordered_map<CallbackGroupKey,
    std::shared_ptr<std::vector<CallbackVarInfo>>> callbackGroupMembers_;
std::atomic<bool> callbackDirty_{false};

// 数据接收队列（无锁）
moodycamel::ConcurrentQueue<std::shared_ptr<PubTableDefine>>    pubTableDefineQueue_;
moodycamel::ConcurrentQueue<std::shared_ptr<TableDataTransfer>> dataTransferQueue_;
```

**关键方法：**

```cpp
bool init(const std::string& nodeName);
bool start();
bool subscribe(const char* nodeName,
               const std::vector<SubscriptionItem>& items,
               DataCallback callback = nullptr);
bool unsubscribe(const char* nodeName,
                 const std::vector<std::string>& varNames);
uint64_t getTotalReceivedVars() const;
void stop();
```

---

## 主要数据结构

### VarMetadata（变量元数据）

发布端和订阅端各自维护一份，含义略有差异。

```cpp
struct VarMetadata {
    uint64_t varHash;                            // 变量名的 64 位 hash（唯一标识）
    BucketManager::BucketIndex bucketIndex;      // 所属桶编号（0~19）
    uint32_t varId;                              // VarStore 中的存储 ID（自增）
    uint32_t currentFreq;                        // 当前推送频率（ms），0xFFFFFFFF 表示无订阅
    uint32_t dataSize;                           // 变量数据大小（bytes）
    std::shared_ptr<DSF::Var::Define> varDefine; // 变量定义（含名称、模型、描述等）

    // 各频率的订阅信息（仅发布端使用）
    struct FreqSub {
        uint32_t freq;      // 订阅频率（ms）
        uint16_t subCount;  // 该频率下的订阅节点数
        uint64_t subMask;   // 订阅节点位掩码（最多 64 个节点，每位对应一个节点）
    };
    std::vector<FreqSub> freqSubs;  // 所有频率的订阅信息
    uint8_t activeFreqCount;        // 当前活跃频率数量
};
```

**频率协商逻辑：**

`currentFreq` 始终取所有 `freqSubs` 中的最小值（最快频率），确保最需要数据的订阅方能及时收到。

```
示例：varA 有两个订阅方
  subNode_A 请求 250ms → freqSubs[0] = { freq:250, subMask:0b01 }
  subNode_B 请求 500ms → freqSubs[1] = { freq:500, subMask:0b10 }
  currentFreq = min(250, 500) = 250ms
```

---

### VarStore（变量数据存储）

无锁的变量值存储，支持高并发读写，核心是 Seqlock 机制。

```cpp
class VarStore {
    // 变量元信息：hash → (id, offset, size)
    struct VarMeta {
        uint64_t hash;
        uint32_t id;
        uint32_t offset;  // 在 arena_ 中的偏移
        uint32_t size;    // 数据大小
    };
    std::vector<VarMeta> metas_;
    std::unordered_map<uint64_t, uint32_t> table_;  // hash → id

    // 数据存储：64字节对齐的连续内存池
    // 每个 Slot 包含 atomic 序列计数器 + 数据
    struct Slot {
        std::atomic<uint32_t> seq;  // 奇数=写入中，偶数=稳定
        uint8_t data[...];          // 实际数据
    };
    uint8_t* arena_;

    // 脏标记：哪些变量有新数据
    std::atomic<bool>* dirty_flags_;
    moodycamel::ConcurrentQueue<uint32_t> dirty_queue_;  // 脏变量 ID 队列

    // 写计数器：订阅端用于检测数据是否更新
    std::atomic<uint32_t>* write_counters_;

    // 重配置保护（注册新变量时使用）
    std::atomic<bool> op_flag_;      // 是否有读操作进行中
    std::atomic<bool> config_flag_;  // 是否正在重配置
};
```

**读写流程（Seqlock）：**

```
写入：
  seq.store(seq+1, release)  // 置为奇数，标记写入中
  memcpy(data, src)
  seq.store(seq+1, release)  // 置为偶数，标记写入完成
  dirty_flags_[id] = true
  dirty_queue_.enqueue(id)

读取：
  loop:
    s1 = seq.load(acquire)
    if (s1 & 1) continue      // 写入中，重试
    memcpy(dst, data)
    s2 = seq.load(acquire)
    if (s1 != s2) continue    // 读取期间被修改，重试
    return true
```

**关键方法：**

```cpp
uint32_t register_var(uint64_t hash, uint32_t size);  // 注册变量，返回 ID
bool     unregister_var(uint64_t hash);               // 软删除（标记 size=0）
bool     finalize();                                  // 分配 arena 和 dirty_flags（注册完成后调用）
bool     write(uint32_t id, const void* src);
bool     write(uint32_t id, const void* src, uint32_t actual_size);
bool     read(uint32_t id, void* dst) const;
ScopedPtr read_ptr(uint32_t id) const;                // 零拷贝读取
void     for_each_dirty(Fn&& fn, size_t batch=1000);  // 遍历脏变量
```

---

### BucketManager（分桶管理）

将变量均匀分配到 20 个桶，每桶对应一个独立 DDS Topic，分散传输压力。

```cpp
class BucketManager {
    static constexpr int ONDEMAND_BUCKET_SIZE = 20;
    using BucketIndex = uint32_t;
    using Member = std::string;  // 变量名

    std::vector<std::unordered_set<Member>> buckets_;  // 20 个桶
    std::unordered_map<Member, uint64_t> table_;       // 变量名 → hash
    size_t total_members_ = 0;
    mutable std::mutex mutex_;
};
```

**分桶算法：**

```cpp
BucketIndex CalculateBucketIndex(const Member& member) {
    return hash(member) % ONDEMAND_BUCKET_SIZE;
}
// 对应 DDS Topic: dsf/var/data/transfer/bucket_{0..19}
```

---

### PublishGroupKey / CallbackGroupKey（调度分组键）

```cpp
// 发布端：按 (桶编号, 频率) 分组
struct PublishGroupKey {
    uint32_t bucketIndex;  // 桶编号（0~19）
    uint32_t freqMs;       // 推送频率（ms）
};

// 订阅端：按频率分组
struct CallbackGroupKey {
    uint32_t freqMs;       // 回调频率（ms）
};
```

---

### GroupVarInfo / CallbackVarInfo（分组内变量信息）

```cpp
// 发布端：每个推送组内的变量信息
struct GroupVarInfo {
    uint64_t varHash;   // 变量 hash
    uint32_t varId;     // VarStore ID
    uint32_t dataSize;  // 数据大小
};

// 订阅端：每个回调组内的变量信息
struct CallbackVarInfo {
    uint64_t varHash;
    int32_t  varId;
    uint32_t dataSize;
    std::string varName;
    DataCallback callback;
    uint32_t lastSeenWriteCount{0};  // 上次回调时的写计数，用于检测变更
};
```

---

### SubscriptionItem / VarCallbackData（用户接口类型）

```cpp
// 订阅请求项
struct SubscriptionItem {
    std::string varName;   // 变量名（格式：pubNodeName_varName）
    uint32_t frequency;    // 期望的回调频率（ms）
};

// 回调数据项
struct VarCallbackData {
    std::string_view varName;   // 变量名（零拷贝）
    const void*      data;      // 数据指针
    size_t           size;      // 数据大小
    uint64_t         timestampNs; // 发布时间戳（纳秒）
};

// 用户回调函数类型
using DataCallback = std::function<void(const std::vector<VarCallbackData>& vars)>;
```

---

### IDL 消息结构

**DSF::Var::Define（变量定义）**

```idl
struct Define {
    string name;              // 变量名
    string modelName;         // 数据模型名
    string modelVersion;      // 模型版本
    string description;       // 描述
    string nodeName;          // 所属节点名
    boolean isReadonly;       // 是否只读
    VarPublishMask publishMask; // PERIODIC / ON_DEMAND / BOTH
    // 可选字段：events, alarms, initialValues, startIndexes, permission
};
```

**DSF::Var::PubTableDefine（变量定义广播）**

```idl
struct PubTableDefine {
    @key string name;                          // 桶名（如 "bucket_0"）
    @key string nodeName;                      // 发布节点名
    string description;
    sequence<PubTableVarDefine> varDefines;   // 该桶内所有变量定义
};
```

**DSF::Message::SubTableRegister（订阅请求）**

```idl
struct SubTableRegister {
    MSGTYPE msgType;                    // SUB_TABLE_REGISTER 或 UNSUB_TABLE_REGISTER
    sequence<DSF::NamedValue> varFreqs; // 变量名 + 频率（字符串形式）
    string tableName;                   // 桶名
    string nodeName;                    // 订阅节点名
    string user;
};
```

**DSF::Var::TableDataTransfer（变量数据推送）**

```idl
struct TableDataTransfer {
    DSF::DT_BLOB mask;                 // Roaring64Map 序列化的变量 hash 集合
    DSF::Timespec timestamp;           // 推送时间戳 { tv_sec, tv_nsec }
    sequence<DSF::DT_BLOB> varData;   // 变量数据数组（与 mask 中的 hash 顺序对应）
    BLOB_TYPE blobType;                // 序列化类型（STRUCTS）
};
```

**DSF::NamedValue / DSF::Timespec**

```idl
struct NamedValue {
    string name;
    string value;
};

struct Timespec {
    long tv_sec;           // Unix 时间戳（秒）
    unsigned long tv_nsec; // 纳秒部分
};
```

---

### TimerScheduler / TimerWheel（时间轮调度器）

```cpp
class TimerScheduler {
    ThreadPool thread_pool_;                    // 工作线程池（默认 8 线程）
    TimerWheel timer_wheel_;                    // 分层时间轮
    std::atomic<bool> should_stop_;
    std::thread timer_thread_;                  // 推进时间轮的专用线程
    Tick tick_ms_;                              // 时钟精度（1ms）
    std::unordered_set<
        std::shared_ptr<TimerEventInterface>> active_timers_;
};
```

**TimerWheel 结构：** 8 层分层时间轮，每层 256 个槽，支持从 1ms 到数天的定时范围。

```cpp
// 创建一次性定时器
std::shared_ptr<TimerEventInterface> Schedule(Callback cb, Tick delay_ticks);

// 创建周期性定时器
std::shared_ptr<TimerEventInterface> ScheduleRecurring(
    Callback cb, Tick delay_ticks, Tick interval_ticks);

// 取消定时器
void Cancel(std::shared_ptr<TimerEventInterface> timer);

// 立即异步执行
void Post(Callback cb);
```

---

## DDS 通信协议

| Topic | 方向 | 消息类型 | 用途 |
|-------|------|----------|------|
| `dsf/sys/var/tableDefine` | Pub → Sub | `PubTableDefine` | 广播变量定义（每桶一条） |
| `dsf/message/commandRequest/subTableRegister` | Sub → Pub | `SubTableRegister` | 发送订阅/取消订阅请求 |
| `dsf/var/data/transfer/bucket_N`（N=0~19） | Pub → Sub | `TableDataTransfer` | 按桶推送变量数据 |

---

## 数据流

### 订阅建立流程

```
Subscriber                                    Publisher
    │                                             │
    │  subscribe("pubNode", items, cb)            │
    │  → 存入 subscriptionCallbacks_              │
    │  → callbackDirty_ = true                   │
    │                                             │
    │──── SubTableRegister (DDS) ───────────────►│
    │     { nodeName, tableName, varFreqs }       │
    │                                             │  processReceiveRegister()
    │                                             │  getOrAssignNodeBit(nodeName) → bit
    │                                             │  更新 VarMetadata.freqSubs[freq].subMask |= bit
    │                                             │  recalcCurrentFreq() → 取最小频率
    │                                             │  schedulerDirty_ = true
    │                                             │
    │                                             │  processPublishTaskScheduler()（50ms 扫描）
    │                                             │  按 (bucketIndex, freqMs) 分组变量
    │                                             │  新增组 → ScheduleRecurring(publishGroupData)
    │                                             │  删除组 → Cancel(timer)
```

### 数据推送流程

```
Publisher TimerScheduler                      Subscriber
    │                                             │
    │  定时器到期（如 250ms）                      │
    │  publishGroupData(bucketIndex=0, freq=250)  │
    │  → 遍历 groupMembers_[{0,250}]              │
    │  → 从 varStore_ 读取各变量数据              │
    │  → 构建 Roaring64Map（变量 hash 集合）       │
    │  → 序列化 TableDataTransfer                 │
    │──── DDS: bucket_0 ──────────────────────►  │
    │                                             │  processDataTransfer()
    │                                             │  反序列化 Roaring64Map → hash 列表
    │                                             │  varStore_.write(varId, data)
    │                                             │  varWriteStamps_[id].writeCount++
    │                                             │  varWriteStamps_[id].timestampNs = ts
    │                                             │
    │                                             │  callbackGroupData(freq=250)（定时器）
    │                                             │  遍历 callbackGroupMembers_[{250}]
    │                                             │  检测 writeCount != lastSeenWriteCount
    │                                             │  构建 VarCallbackData 列表
    │                                             │  调用用户 callback(vars)
```

### 取消订阅流程

```
Subscriber                                    Publisher
    │                                             │
    │  unsubscribe("pubNode", varNames)           │
    │  → 从 subscriptionCallbacks_ 移除           │
    │  → callbackDirty_ = true                   │
    │                                             │
    │──── SubTableRegister (UNSUB, DDS) ────────►│
    │                                             │  handleUnsubscribe()
    │                                             │  freqSubs[freq].subMask &= ~bit
    │                                             │  subCount--
    │                                             │  若 subCount==0 → 移除该 FreqSub
    │                                             │  recalcCurrentFreq()
    │                                             │  若无订阅 → currentFreq = 0xFFFFFFFF
    │                                             │  schedulerDirty_ = true → 取消定时器
```

---

## 性能设计

| 优化点 | 实现方式 |
|--------|----------|
| 分桶分散压力 | 20 个 Bucket，对应 20 个 DDS Topic，并行推送 |
| 无锁存储 | VarStore 使用 Seqlock（atomic 序列计数器），读写无锁 |
| 批量推送 | 同 (桶, 频率) 的变量合并为一次 DDS 消息 |
| 压缩传输 | Roaring64Map 压缩变量 hash 集合，减少消息体积 |
| 增量调度 | 50ms 扫描一次，只在订阅变化时重建定时器 |
| 无锁队列 | moodycamel ConcurrentQueue 用于线程间传递消息 |
| 读写分离 | varIndex_ 使用 shared_mutex，读多写少场景高效 |
| 零拷贝读取 | VarStore::read_ptr() 返回 ScopedPtr，避免数据拷贝 |
| 变更检测 | writeCount 计数器，回调时只处理有新数据的变量 |

---

## 使用示例

### 发布端

```cpp
OnDemandPub pub;
pub.init("pubNode");
pub.start();

// 注册 10 万个变量
std::vector<DSF::Var::Define> vars;
for (int i = 0; i < 100000; ++i) {
    DSF::Var::Define def;
    def.varDefine().name("var" + std::to_string(i));
    def.varDefine().nodeName("pubNode");
    def.varDefine().modelName("int");
    vars.push_back(def);
}
pub.createVars(vars);

// 持续更新数据
while (running) {
    for (int i = 0; i < 100000; ++i) {
        pub.setVarData(("var" + std::to_string(i)).c_str(), &i, sizeof(i));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
pub.stop();
```

### 订阅端

```cpp
OnDemandSub sub;
sub.init("subNode");
sub.start();

// 订阅 5 万个变量，250ms 频率
std::vector<SubscriptionItem> items;
for (int i = 0; i < 50000; ++i) {
    items.push_back({"pubNode_var" + std::to_string(i), 250});
}

sub.subscribe("pubNode", items, [](const std::vector<VarCallbackData>& vars) {
    for (auto& var : vars) {
        // var.varName    - 变量名
        // var.data       - 数据指针
        // var.size       - 数据大小
        // var.timestampNs - 发布时间戳（纳秒）
    }
});

// 取消订阅
std::vector<std::string> names = {"pubNode_var0", "pubNode_var1"};
sub.unsubscribe("pubNode", names);

sub.stop();
```

---

## 已知限制 & TODO

1. IDL 中变量大小信息待完善（目前依赖模型定义推断）
2. Key 类型优化，支持增量传输（只传变化的变量）
3. Listener 生命周期优化0
4. 极端场景（1亿变量、单频率、单 Bucket）的负载均衡
5. 支持相位偏移，实现同频率触发的时间对齐
6. 节点数上限 64（subMask 为 uint64_t）
