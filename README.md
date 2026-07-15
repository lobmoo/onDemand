> **版本**: v1.0 | **最后更新**: 2026-07-15 | **作者**: wwk
>

---

## 第一部分：项目概述
### 1.1 一句话定义
OnDemand 是面向**工业自动化场景**的高性能按需发布-订阅中间件——对标 IEC 61850 MMS/GOOSE、IEC 60870-5-104、OPC UA PubSub 等工业通信标准，在 DDS 之上实现**逐变量、逐订阅者的动态频率协商**，解决 SCADA/DCS 系统中 10 万+ 测点的按需分发问题。

#### ① 动态模型发现（最核心差异）
> **无需预配置文件，发布者运行时广播模型，订阅者自动发现并解析。**
>
> <!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784098437976-bad3224c-7e76-459a-9ed0-089d1b1733fe.png)
>

+ 发布者调用 `createVars()` / `registerVars()` 注册变量，系统自动广播 `PubTableDefine`
+ 订阅者通过 `setTableDefineCallback()` 接收变量定义，无需任何预配置
+ 变量支持运行时动态增删（`createVars` / `deleteVars`），模型实时更新
+ 变量定义包含：名称、大小、模型名、版本、描述、读写权限——兼容 IEC 61850 DO/DA 数据模型

#### ② 变量级频率协商
> **订阅者按需声明每个变量的期望周期，发布者自动取最小值发布，取消时自动上调。**
>

```plain
场景示例（变量 "busbar_voltage"）:
  保护装置 subscribe(10ms)  → currentFreq = 10ms  ← 按 10ms 发布
  HMI       subscribe(1s)   → currentFreq = 10ms  ← 不变（已是最小值）
  历史库    subscribe(10s)  → currentFreq = 10ms  ← 不变

  保护装置 unsubscribe()    → currentFreq = 1s    ← 自动上调到次小值
  HMI       unsubscribe()   → currentFreq = 10s   ← 继续上调
  历史库    unsubscribe()   → currentFreq = 0xFFFF ← 无订阅者，停止发布
```

+ 对标 IEC 61850 Report Control Block 的按需报告，但粒度从**数据集级**细化到**单变量级**
+ 频率变更对发布者透明，无需人工干预

#### ③ 大规模 + 高性能
+ 10 万+ 变量单节点，对标大型变电站/工厂测点规模
+ 20 桶分片并行传输，避免单链路瓶颈
+ Seqlock 双缓冲零拷贝读取，微秒级延迟
+ DDS 提供可靠传输、断网重连、Participant 存活检测

### 1.4 与工业标准的对比
| 特性 | IEC 61850 GOOSE | IEC 61850 RCB | IEC 104 | OPC UA PubSub | **OnDemand** |
| --- | --- | --- | --- | --- | --- |
| 传输机制 | 二层组播 | MMS/TCP | TCP 轮询 | UDP 多播 | **DDS** |
| **配置方式** | **CID 文件预装** | **CID 文件预装** | **点表预装** | **信息模型预共享** | **运行时动态发现** |
| **模型更新** | 停机改配 | 停机改配 | 双方改点表 | 重新发布模型 | **运行时热更新** |
| 频率控制 | 固定周期 | 报告使能+触发选项 | 固定轮询 | 发布者决定 | **订阅者协商** |
| 协商粒度 | 无 | 数据集级 | 站级 | Topic 级 | **变量级** |
| 变量规模 | ~千级 | ~千级 | ~万级 | ~万级 | **10 万+** |
| 多订阅者 | 广播 | 1:1 连接 | 1:1 连接 | 1:N | **N:M 动态协商** |
| 频率自适应 | 无 | 条件触发 | 无 | 无 | **自动上调/下调** |
| 零拷贝读取 | 无 | 无 | 无 | 无 | **Seqlock 双缓冲** |


### 1.5 关键技术指标
| 指标 | 目标值 | 工业对标 |
| --- | --- | --- |
| 变量规模 | 10 万+ 单节点 | 大型变电站测点规模 |
| 频率精度 | 1ms tick（时间轮） | 满足保护级 3ms 要求 |
| 读取延迟 | 零拷贝（Seqlock + 双缓冲） | 微秒级读取，满足实时控制 |
| 并发写入 | 无锁写 + CAS 脏标记 | 多数据源并发采集 |
| 传输并行度 | 20 桶 × 独立 DDS Topic | 避免单链路瓶颈 |
| 最大订阅者 | 64 节点（subMask 位掩码） | 满足多客户端场景 |
| 频率协商 | 变量级，自动上调/下调 | 超越现有工业标准 |
| **模型发现** | **运行时动态广播，零配置** | **消除 CID/点表预装** |


---

## 第二部分：整体架构
### 2.1 系统拓扑图
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095421737-9ac6cc4f-48f1-4a65-843b-0b7268496ae1.png)

### 2.2 核心模块划分
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095475260-7b640f2c-9947-49cd-8172-2069720c61da.png)

### 2.3 DDS Topic 通信拓扑
| Topic 名称 | 方向 | 消息类型 | 用途 | QoS |
| --- | --- | --- | --- | --- |
| `dsf/sys/var/tableDefine` | Pub → Sub | `PubTableDefine` | 广播变量定义（按 bucket 分批） | RELIABLE, TRANSIENT_LOCAL |
| `dsf/message/commandRequest/subTableRegister` | Sub → Pub | `SubTableRegister` | 订阅/取消订阅请求 | RELIABLE, TRANSIENT_LOCAL |
| `dsf/var/data/transfer/bucket_N` (N=0–19) | Pub → Sub | `TableDataTransfer` | 批量变量数据（20 个独立 topic） | BEST_EFFORT (异步) |


---

## 第三部分：核心模块详解
### 3.1 OnDemandPub（发布者）
**职责**: 变量注册、订阅请求处理、频率协商、定时批量数据发布。

#### 关键数据结构
```cpp
// 变量索引: hash -> 元数据
std::unordered_map<uint64_t, VarMetadata> varIndex_;

// 发布分组: (bucketIndex, freqMs) -> 定时器 + 成员列表
struct PublishGroupKey {
    uint32_t bucketIndex;
    uint32_t freqMs;
};
std::unordered_map<PublishGroupKey, PublishGroupEntry> groupMembers_;

// 轻量变量索引（registerVars 懒创建用）
std::array<LiteBucket, 20> liteBucketMembers_;
```

#### 线程模型
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095531269-50441904-60db-4016-90af-26b507402efb.png)

#### 核心流程：订阅处理
```cpp
// processReceiveRegister() 伪代码
while (running) {
    data = normalQueue.try_dequeue() ?: retryQueue.pop();  // 优先正常队列
    if (!data) { sleep(100ms); continue; }

    missing = handleSubscribe(nodeName, varFreqs);
    if (missing > 0) {
        // 指数退避：200ms → 400ms → 800ms → ... → 3000ms (cap)
        retryQueue.push(onlyMissingVars(data));
    }
}
```

#### 核心流程：数据发布
```cpp
// publishGroupData(bucketIndex, freqMs) 伪代码
// 1. 收集该组内所有 dirty 变量
ids = collectDirtyIds(groupMembers[{bucketIndex, freqMs}]);

// 2. 批量零拷贝读
varStore_.read_batch(ids, count, [&](i, ptr, size) {
    // 3. 序列化到 RoaringBitmap mask + varData[]
    mask.add(ids[i]);
    varData.push_back(ptr, size);
});

// 4. 通过 DDS Writer 发送 TableDataTransfer
writer->writeMessage({mask.serialize(), timestamp, varData, blobType});
```

### 3.2 OnDemandSub（订阅者）
**职责**: 接收变量定义、发送订阅请求、接收数据存储到 VarStore、按频率触发用户回调。

#### 关键数据结构
```cpp
// 变量索引: hash -> 元数据（含 varId 用于 VarStore 读取）
std::unordered_map<uint64_t, VarMetadata> varIndex_;

// 回调分组: (bucketIndex, freqMs) -> 定时器 + 成员 + 用户回调
std::unordered_map<CallbackGroupKey, CallbackGroupEntry> callbackGroupMembers_;

// 写入时间戳/计数（按 bucket 粒度，lock-free）
VarWriteStamp varWriteStamps_[20];

// PubTableDefine 缓存（用于断网重连恢复）
std::unordered_map<string, vector<shared_ptr<PubTableDefine>>> pubTableDefineCache_;
```

#### 线程模型
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095555750-651bfee1-8fe1-4e93-96eb-f4cd9db15fdc.png)

#### 核心流程：数据接收与回调
```cpp
// onReceiveDataTransferCb() — DDS 回调线程
void onReceiveDataTransferCb(topic, data) {
    // 1. 解析 RoaringBitmap mask
    mask = Roaring64Map::deserialize(data->mask());

    // 2. 遍历 mask 中的变量，写入 VarStore
    for (varHash : mask) {
        meta = varIndex_[varHash];
        varStore_.write(meta.varId, data->varData()[i]);
        varWriteStamps_[bucket].writeCount++;  // atomic
    }

    // 3. 更新时间戳
    varWriteStamps_[bucket].timestampNs = data->timestamp();
}

// callbackGroupData(bucketIndex, freqMs) — 定时器触发
void callbackGroupData(bucketIndex, freqMs) {
    // 1. 检查 writeCount 是否变化（跳过无新数据的回调）
    if (currentWriteCount == lastWriteCount) return;

    // 2. 批量零拷贝读取
    vector<VarCallbackData> results;
    varStore_.read_batch(ids, count, [&](i, ptr, size) {
        results.push_back({varName, varType, ptr, size, timestamp});
    });

    // 3. 调用用户回调
    callback(results);
}
```

### 3.3 VarStore（无锁变量存储）
**职责**: 高性能、线程安全的变量值存储，支持零拷贝读取。

#### 内存布局
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095595091-2a24493a-5415-42f2-8529-b0abf7718cbc.png)

#### Seqlock 协议
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095660565-2da4b1a0-3532-4b24-9f41-c0625bdc946b.png)

#### 线程安全模型
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095687304-fb47b4ad-4e59-48d4-82ec-d4ea4b40452c.png)

### 3.4 BucketManager（桶管理）
**职责**: 将变量哈希均匀分配到 20 个桶中，每个桶对应一个独立的 DDS Topic。

```cpp
class BucketManager {
    // 核心：hash % 20 → bucket index
    static BucketIndex CalculateBucketIndexFromHash(uint64_t hash) {
        return hash % ONDEMAND_BUCKET_SIZE;  // ONDEMAND_BUCKET_SIZE = 20
    }

    // 每个桶存储该桶内所有变量的 hash 集合
    BucketTable buckets_;  // vector<unordered_set<uint64_t>>
};
```

**设计要点**:

+ 20 个桶 × 独立 DDS Topic → 并行传输，避免单 Topic 成为瓶颈
+ 桶数量固定为 20（编译期常量），平衡 Topic 数量与并行度
+ 变量分配基于 FNV-1a 哈希取模，天然均匀分布

### 3.5 TimerScheduler / TimerWheel（定时器系统）
**职责**: 提供毫秒级精度的周期性定时器，驱动数据发布和回调调度。

#### 架构
<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095705578-ea6b9fd5-fadd-44fd-aac4-0b75a4ee2f26.png)

#### 执行模式
| 模式 | 场景 | 线程池 | 说明 |
| --- | --- | --- | --- |
| `direct_execute = true` | Pub 端 | 无 | 回调在 timer 线程直接执行，无线程池开销 |
| `direct_execute = false` | Sub 端 | 有 | 回调投递到线程池，避免阻塞 timer 线程 |


#### 关键实现
```cpp
// RecurringCallbackTimer::execute() — 周期性定时器
void execute() {
    // 1. 在 timer 线程内重调度（已持锁，无竞争）
    scheduler_.RescheduleRecurringNoLock(this, interval_ticks_);

    // 2. 将回调放入 pendingTasks_（不触发 syscall）
    scheduler_.pendingTasks_.push_back([this]() {
        if (!is_canceled_) callback_();
    });
}

// TimerLoop() — 主循环
void TimerLoop() {
    while (!should_stop_) {
        if (now >= next_tick && !is_paused_) {
            lock();
            timer_wheel_.advance(1, kMaxExecutePerTick);  // 触发到期事件
            unlock();

            // 锁外执行回调（direct 或投递线程池）
            for (auto& task : pendingTasks_) {
                direct_execute_ ? task() : thread_pool_.enqueue(task);
            }
            pendingTasks_.clear();
        }
        sleep_until(next_tick);
    }
}
```

### 3.6 DDS Wrapper（通信抽象层）
**职责**: 编译期切换 FastDDS / TXDDS 后端，提供统一的读写接口。

<!-- 这是一张图片，ocr 内容为： -->
![](https://cdn.nlark.com/yuque/0/2026/png/28969906/1784095738312-0e3edce3-27d1-44c8-b5fc-21bfa4eb3e2f.png)

---

## 第四部分：通信链路详解
### 4.1 订阅建立流程
### 4.2 数据推送流程
<!-- 这是一个文本绘图，源码为：sequenceDiagram
    participant User as 用户代码
    participant Pub as Publisher
    participant Timer as TimerScheduler
    participant DDS as DDS Network
    participant Sub as Subscriber
    participant CB as 用户回调

    User->>Pub: setVarData("varA", data, size)
    Pub->>Pub: VarStore.write() → seqlock 双缓冲写入<br/>dirty_queue_.enqueue(id)

    Timer->>Pub: 定时器触发 publishGroupData(bucket, freq)
    Pub->>Pub: read_batch() → 批量零拷贝读 dirty 变量
    Pub->>Pub: 序列化 RoaringBitmap mask + varData[]

    Pub->>DDS: DDSTopicWriter.write(TableDataTransfer)
    DDS->>Sub: onReceiveDataTransferCb

    Sub->>Sub: 解析 mask → VarStore.write() 批量写入<br/>writeCount++ (atomic)

    Note over Sub: CallbackScheduler 定时器触发
    Sub->>Sub: callbackGroupData():<br/>检查 writeCount 变化 → read_batch()
    Sub->>CB: DataCallback(vector<VarCallbackData>) -->
![](https://cdn.nlark.com/yuque/__mermaid_v3/eaae819880b5c90840697fcac9cce4aa.svg)

### 4.3 频率协商流程
<!-- 这是一个文本绘图，源码为：sequenceDiagram
    participant Sub1 as Subscriber A (10ms)
    participant Sub2 as Subscriber B (100ms)
    participant Pub as Publisher

    Note over Pub: varX.currentFreq = 0xFFFFFFFF (无订阅)

    Sub1->>Pub: subscribe(varX, 10ms)
    Pub->>Pub: freqSubs: [{10ms, count=1}]<br/>currentFreq = 10ms<br/>调度 10ms 定时器

    Sub2->>Pub: subscribe(varX, 100ms)
    Pub->>Pub: freqSubs: [{10ms, count=1}, {100ms, count=1}]<br/>currentFreq = min(10, 100) = 10ms (不变)

    Note over Pub: 以 10ms 频率发布 varX

    Sub1->>Pub: unsubscribe(varX)
    Pub->>Pub: freqSubs: [{100ms, count=1}]<br/>currentFreq = 100ms<br/>重调度定时器

    Note over Pub: 以 100ms 频率发布 varX

    Sub2->>Pub: unsubscribe(varX)
    Pub->>Pub: freqSubs: []<br/>currentFreq = 0xFFFFFFFF (无订阅者)<br/>取消定时器 -->
![](https://cdn.nlark.com/yuque/__mermaid_v3/ec47e8d7ae08db105e32c78494032e15.svg)

### 4.4 断网重连流程
<!-- 这是一个文本绘图，源码为：sequenceDiagram
    participant Sub as Subscriber
    participant DDS as DDS Network
    participant Pub as Publisher (新实例)

    Note over Sub: onParticipantDiscovery(DROPPED)<br/>检测到旧 Pub 掉线

    Sub->>Sub: forceUnsubscribeNode(oldMask)<br/>清理旧变量定义

    Note over Pub: 新 Pub 实例启动
    Pub->>DDS: 广播 PubTableDefine (新实例)
    DDS->>Sub: onReceiveTableDefineCb

    Sub->>Sub: 检测到同名 Pub 新实例<br/>从 pubTableDefineCache_ 恢复定义<br/>restorePubTableDefineFromCache()

    Sub->>Sub: resendSubscriptions()<br/>重发所有订阅请求

    Sub->>DDS: SubTableRegister (全部订阅)
    DDS->>Pub: handleSubscribe → 恢复发布 -->
![](https://cdn.nlark.com/yuque/__mermaid_v3/3251ea6215c12888e4c9d351c4bca2bb.svg)

---

## 第五部分：优秀设计亮点
### 5.1 Seqlock + 双缓冲的无锁读写
**问题**: 10 万变量的高频读写场景，传统互斥锁会导致严重的锁竞争和上下文切换。

**方案**: 每个变量 Slot 使用 Seqlock + 双缓冲区：

+ **写端**: 始终写非活跃缓冲区（`1 - committed`），写完原子翻转 `committed`
+ **读端**: 读 `committed` 指向的缓冲区，用 seqlock 验证读取期间无翻转
+ **零拷贝**: `read_zero_copy()` 返回直接指向 arena 内存的指针，无 memcpy

**为什么好**:

+ 读操作完全无锁（仅 atomic load），适合多读少写场景
+ 双缓冲保证读端永远不会读到半写状态
+ 64 字节对齐避免 false sharing

**关键代码路径**: [variable_store.h:226-259](core/ondemand/variable_store.h#L226-L259) (`write`), [variable_store.h:365-429](core/ondemand/variable_store.h#L365-L429) (`ZeroCopyReadHandle`)

### 5.2 Dirty Flag + 延迟重建的定时器管理
**问题**: 每次订阅变更都重建所有定时器，10 万变量 × 频繁订阅 = 定时器风暴。

**方案**:

+ `schedulerDirty_` 原子标记，订阅变更时仅置位
+ 50ms 后台扫描循环检测 dirty，diff 当前定时器组与期望组
+ 仅取消/新增差异定时器，避免全量重建

**为什么好**:

+ 批量订阅变更只触发一次重建
+ diff 算法避免取消/重建未变化的定时器
+ 50ms 延迟合并高频订阅请求

**关键代码路径**: [on_demand_pub.h:406](core/ondemand/on_demand_pub.h#L406) (`schedulerDirty_`), `processPublishTaskScheduler()`

### 5.3 频率协商的最小值策略
**问题**: 多个订阅者对同一变量有不同频率需求，需要找到最优发布频率。

**方案**:

+ `VarMetadata.freqSubs` 维护 `(freq, count, subMask)` 列表
+ `currentFreq = min(all freqSubs)` — 始终取最小值
+ 取消订阅时：`count--`，若 `count == 0` 移除该频率条目，重新计算 min

**为什么好**:

+ O(1) 查找当前频率（直接读 `currentFreq`）
+ 支持最多 64 个订阅者（`subMask` 位掩码）
+ 频率变更仅影响受影响的定时器组

### 5.4 20 桶分片 + RoaringBitmap 批量传输
**问题**: 单 DDS Topic 传输 10 万变量数据成为瓶颈。

**方案**:

+ `BucketManager` 按 `hash % 20` 分桶，每桶独立 DDS Topic
+ `TableDataTransfer` 使用 RoaringBitmap 序列化变量掩码，仅传输有变化的变量
+ `varData[]` 按 mask 顺序排列，接收端按序解析

**为什么好**:

+ 20 个 Topic 并行传输，带宽利用率提升 20 倍
+ RoaringBitmap 压缩率高（稀疏位图仅占几字节）
+ 仅传输 dirty 变量，减少无效数据

### 5.5 指数退避重试队列
**问题**: 订阅请求可能在变量创建之前到达（时序竞争），直接丢弃会导致订阅丢失。

**方案**:

+ 正常队列 (`ConcurrentQueue`) + 重试队列 (`std::queue + mutex`)
+ 优先消费正常队列，空了再消费重试队列
+ 指数退避：200ms → 400ms → 800ms → ... → 3000ms (cap)
+ 重试消息仅包含缺失变量，避免重复处理

**为什么好**:

+ 不丢弃任何订阅请求
+ 指数退避避免忙等
+ 重试队列与正常队列隔离，不阻塞新消息

### 5.6 编译期 DDS 后端切换
**问题**: 不同部署环境使用不同 DDS 实现（FastDDS / TXDDS），需要无缝切换。

**方案**:

+ CMake 选项 `-DUSE_FASTDDS=ON` / `-DUSE_TXDDS=ON`
+ `dds_abstraction.h` 统一类型别名
+ 模板化的 `DDSTopicWriter<T>` / `DDSTopicReader<T>` 适配不同后端

**为什么好**:

+ 零运行时开销（编译期绑定）
+ 上层代码完全无感知后端差异
+ 易于扩展新后端

### 5.7 RAII 全覆盖的资源管理
**问题**: DDS 资源（Participant、Writer、Reader）生命周期管理复杂，手动释放易遗漏。

**方案**:

+ `DataNode` 封装 Participant 生命周期
+ `DDSTopicWriter<T>` / `DDSTopicReader<T>` 使用 `shared_ptr` 管理
+ `OpGuard` / `ConfigGuard` RAII 守卫管理 VarStore 并发访问
+ `ZeroCopyReadHandle` RAII 管理 `op_enter/op_exit`

**为什么好**:

+ 异常安全：栈展开时自动释放
+ 无资源泄漏风险
+ 代码简洁，无手动 cleanup

### 5.8 moodycamel 无锁队列的生产者-消费者模式
**问题**: DDS 回调线程与业务处理线程之间需要高效、线程安全的数据传递。

**方案**:

+ `moodycamel::ConcurrentQueue` 用于 DDS 回调 → 处理线程的数据传递
+ 单生产者（DDS 回调线程）、多消费者（处理线程）模式
+ 无锁设计，高并发下性能优于 `std::mutex + std::queue`

**为什么好**:

+ 无锁，避免 DDS 回调线程阻塞
+ 批量 dequeue 支持（`try_dequeue_bulk`）
+ 适合高频数据传输场景

---

## 第六部分：线程模型总览
### 6.1 全局线程清单
| 线程名 | 所属模块 | 职责 | 数量 |
| --- | --- | --- | --- |
| `PubRegProc` | OnDemandPub | 处理订阅注册请求（从 ConcurrentQueue 消费） | 1 |
| `PubScheduler` | OnDemandPub | 定时器主循环，触发 publishGroupData | 1 |
| `FreqChangeCb` | OnDemandPub | 频率变化回调派发 | 1 |
| `TimerScheduler` | TimerScheduler | 定时器主循环（1ms tick） | 1/模块 |
| `SubTableDefine` | OnDemandSub | 处理变量定义（从 ConcurrentQueue 消费） | 1 |
| `SubDataTransfer` | OnDemandSub | 处理数据传输（从 ConcurrentQueue 消费） | 1~N |
| `CallbackScheduler` | OnDemandSub | 回调定时器主循环 | 1 |
| ThreadPool Worker | TimerScheduler | 执行用户回调（sub 场景） | hardware_concurrency/4 |
| DDS 内部线程 | FastDDS/TXDDS | DDS 发现、收发、回调 | 若干 |


### 6.2 线程间通信方式
| 生产者 | 消费者 | 通信方式 | 用途 |
| --- | --- | --- | --- |
| DDS 回调 | PubRegProc | `moodycamel::ConcurrentQueue` | 订阅请求传递 |
| DDS 回调 | SubTableDefine | `moodycamel::ConcurrentQueue` | 变量定义传递 |
| DDS 回调 | SubDataTransfer | `moodycamel::ConcurrentQueue` | 数据传输传递 |
| PubRegProc | FreqChangeCb | `moodycamel::ConcurrentQueue` | 频率变化通知 |
| PubRegProc | PubScheduler | `atomic<bool> schedulerDirty_` | 脏标记通知 |
| DDS 回调 | Sub (重连) | `ParticipantListener` | Participant 上下线检测 |


### 6.3 锁策略与锁序
| 锁 | 保护对象 | 类型 | 持有时间 |
| --- | --- | --- | --- |
| `varIndexMutex_` | `varIndex_` | `shared_mutex` | 读多写少，读锁热路径 |
| `DataTransferWriterMapMutex_` | `dataTransferWriterMap_` | `mutex` | 仅创建时持有 |
| `publishGroupsMutex_` | `groupMembers_`, `publishGroupTimers_` | `mutex` | 调度器扫描 |
| `freqChangeCbMutex_` | `freqChangeCb_` | `mutex` | 回调设置 |
| `VarStore::reconfig_mu_` | arena 重分配 | `mutex + cv` | config 期间阻塞 op |
| `VarStore::config_mu_` | config 互斥 | `mutex` | 注册/扩容 |


**锁序规则**: `varIndexMutex_` → `publishGroupsMutex_`（禁止反序）

---

## 第七部分：部署与配置
### 7.1 编译选项
```bash
source .env                          # 设置 LD_LIBRARY_PATH
cd build
cmake .. -DUSE_TXDDS=ON              # 或 -DUSE_FASTDDS=ON
make -j$(nproc)
```

| CMake 选项 | 说明 | 默认值 |
| --- | --- | --- |
| `-DUSE_TXDDS=ON` | 使用 TXDDS 后端 | OFF |
| `-DUSE_FASTDDS=ON` | 使用 FastDDS 后端 | OFF |


### 7.2 DDS Domain / QoS 配置
| 参数 | 值 | 说明 |
| --- | --- | --- |
| Domain ID | 66 | DDS 域 ID |
| Discovery Multicast | 239.255.0.1:7400 | PDP 多播地址 |
| User Multicast | 239.255.0.1:7401 | 用户数据多播 |
| Discovery Keep Alive | 3000ms / 1000ms | PDP 心跳间隔/超时 |
| Initial Announcements | 30 / 100ms | 初始公告次数/间隔 |


### 7.3 关键可调参数
| 参数 | 定义位置 | 默认值 | 说明 |
| --- | --- | --- | --- |
| 桶数量 | `ONDEMAND_BUCKET_SIZE` | 20 | 分桶数，影响并行度和 Topic 数量 |
| 最大订阅者数 | `maxNodeNum_` | 64 | 受 subMask (uint64_t) 限制 |
| 每节点最大变量数 | `maxVarsPerNode_` | 0 (无限) | 资源限制 |
| 定时器 tick | `TimerScheduler(tick_ms)` | 1ms (pub) / 10ms (sub) | 定时器精度 |
| 调度器重建延迟 | `processPublishTaskScheduler` 扫描间隔 | 50ms | dirty 后延迟重建 |
| 重试退避范围 | `kMinPartialRetryMs` ~ `kMaxRetryMs` | 200ms ~ 3000ms | 指数退避范围 |
| DDS Writer History Depth | `createTableDefineWriter` | 1 (定义) / 64 (注册) | QoS 历史深度 |


---

## 附录：IDL 类型定义
### 核心类型
```plain
// 变量定义
struct Define {
    string name;           // 变量名称
    int32 size;            // 变量大小
    string modelName;      // 模型名称
    string modelVersion;   // 模型版本
    string description;    // 描述
    string nodeName;       // 创建节点名称
    boolean isReadonly;    // 是否只读
    VarPublishMask publishMask;  // 发布能力掩码
};

// 表定义（批量变量定义）
struct PubTableDefine {
    @key string name;      // 表名称
    @key string nodeName;  // 所属节点名称
    string description;    // 表描述
    sequence<PubTableVarDefine> varDefines;  // 表内变量定义
};

// 表数据传输
struct TableDataTransfer {
    DSF::DT_BLOB mask;           // RoaringBitmap 序列化掩码
    DSF::Timespec timestamp;     // 数据发布时间
    sequence<DSF::DT_BLOB> varData;  // 变量数据数组
    BLOB_TYPE blobType;          // 序列化类型
};

// 订阅注册请求
struct SubTableRegister {
    MSGTYPE msgType;
    sequence<DSF::NamedValue> varFreqs;  // 变量名 + 频率(ms)
    string tableName;
    string nodeName;
    string user;
};
```

