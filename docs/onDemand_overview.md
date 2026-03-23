# onDemand 按需发布订阅系统 — 项目概述

## 1. 项目定位

基于 DDS（数据分发服务）的大规模变量按需发布订阅框架。支持 **10 万级变量** 的高效发布与订阅，通过分桶隔离、时间轮调度、无锁存储等机制，实现低延迟、高吞吐的数据传输。

核心场景：工业控制系统、实时仿真等需要大量变量按不同频率发布/订阅的场景。

---

## 2. 解决什么问题

传统 DDS 发布订阅模式中，所有变量共享同一个定时器、同一个数据通道。当变量数量达到 10 万级别时：

- **定时器瓶颈**：单定时器无法为不同桶的变量提供差异化调度
- **数据通道竞争**：所有变量挤在同一个 Topic 上，序列化/反序列化开销大
- **无法按需订阅**：订阅端只能全量接收，无法指定"只订阅某些变量、以特定频率"

onDemand 的解决方案：**hash 分桶 + 独立通道 + 按频率分组调度**

---

## 3. 整体架构

```
┌─────────────────────────────────────────────────┐
│                  用户业务层                       │
│   pub.setVarData() / sub.subscribe(callback)    │
├─────────────────────────────────────────────────┤
│              OnDemandPub / OnDemandSub           │
│  pub: createVars → publishGroupData → DDS Write  │
│  sub: subscribe → processDataTransfer → callback │
├──────────────────┬──────────────────────────────┤
│   VarStore       │     TimerScheduler           │
│  seqlock+双缓冲   │   8 级时间轮 + 线程池         │
│  无锁读写         │   fixed-rate 周期调度          │
├──────────────────┴──────────────────────────────┤
│            DDS 通信抽象层 (DdsWrapper)            │
│   编译时切换: FastDDS / TXDDS                     │
│   DataWriter / DataReader / Participant          │
├─────────────────────────────────────────────────┤
│               FastDDS / TXDDS                    │
│            (底层 DDS 实现)                        │
└─────────────────────────────────────────────────┘
```

### 分层说明

| 层 | 职责 | 关键文件 |
|---|------|---------|
| 业务层 | 变量注册、订阅、回调分组 | `on_demand_pub.cc`, `on_demand_sub.cc` |
| 存储层 | 无锁双缓冲变量存储 | `variable_store.h` |
| 调度层 | 时间轮定时器 + 异步执行 | `timer_scheduler.h`, `timer_wheel.h` |
| 通信层 | DDS 封装，编译时切换底层实现 | `fastdds_node.h`, `dds_abstraction.h` |

---

## 4. 分桶机制

### 为什么分桶

将 10 万变量按 hash 分成 20 个桶（`ONDEMAND_BUCKET_SIZE = 20`），每个桶：

- 独立的 DDS Topic（`dsf/var/data/transfer/bucket_N`）
- 独立的 DataWriter（pub）/ DataReader（sub）
- 独立的定时器和调度周期

### 分桶逻辑

```
bucketIndex = hash(varName) % 20
```

每桶约 5000 个变量。pub 端逐桶发送数据，避免所有变量同时序列化造成的 CPU 尖峰。

### 调度对齐（Jitter）

pub 和 sub 的定时器触发时机通过 jitter 对齐：

```
jitter = bucketIdx * (freqMs / ONDEMAND_BUCKET_SIZE) + 5ms
```

- pub 的 bucket0 在 `freqMs + 0` 触发，bucket1 在 `freqMs + 25` 触发...
- sub 的对应 bucket 在 pub 发完后触发（+5ms 余量），确保 sub 读到的是当周期数据

---

## 5. VarStore — 无锁变量存储

基于 **seqlock + 双缓冲** 的无锁并发存储，支持一写多读。

### 内存布局

```
Arena: [Slot0][Slot1][Slot2]...  (64 字节对齐分配)

Slot 结构:
┌──────┬──────────┬─────────────────────┐
│ seq  │ committed│   data[0]   │ data[1] │
│u32   │  u8      │   size bytes │size bytes│
└──────┴──────────┴─────────────────────┘
```

- `seq`：原子计数器，奇数表示"正在写入"，偶数表示"稳定可读"
- `committed`：0 或 1，指向当前可读的缓冲区
- `data[0]` / `data[1]`：双缓冲，写入非 committed 缓冲，写完翻转

### 写入流程

```
seq += 1  (奇数=写入中)
memcpy(data[1-committed], src, size)
committed = 1 - committed  (翻转)
seq += 1  (偶数=稳定)
```

### 读取流程

```
do {
    s = seq.load()
    if (s & 1) continue;  // 正在写入，重试
    c = committed.load()
    memcpy(dst, data[c], size)
} while (seq.load() != s);  // seq 变了说明写入发生，重试
```

最多重试 10 次，实际上写入竞争极少时 1-2 次即可完成。

### 其他能力

- `write_batch()`：批量写入，单次 seqlock 保护
- `ZeroCopyReadHandle`：RAII 零拷贝读取，直接返回 arena 指针
- `for_each_dirty()`：遍历 dirty 队列，用于 pub 端检测哪些变量有更新
- `finalize()`：一次性分配 aligned arena，支持动态扩展

---

## 6. 时间轮调度

### TimerWheel（底层）

8 级层次化时间轮，每级 256 个槽（基于 Ratas 库）。总覆盖范围：256^8 ticks。事件按延迟大小自动选择层级，短延迟在低层级（精确），长延迟在高层级。

### TimerScheduler（封装层）

将 TimerWheel + ThreadPool + 专用定时器线程封装为易用的调度器：

| 方法 | 语义 |
|------|------|
| `Schedule(cb, delay)` | 单次执行 |
| `ScheduleRecurring(cb, delay, interval)` | 周期执行（fixed-rate：先重排再回调） |
| `Post(cb)` | 通过线程池立即异步执行 |
| `Cancel(timer)` | 取消定时器 |

定时器线程每 1ms tick 一次，最多执行 20 个定时器（防止阻塞）。回调通过 ThreadPool 异步执行，不阻塞定时器循环。

### Pub/Sub 中的使用

- **Pub**：每个 `(bucketIdx, freqMs)` 组合创建一个 `ScheduleRecurring` 定时器，触发 `publishGroupData()`
- **Sub**：每个 `(bucketIdx, freqMs)` 组合创建一个 `ScheduleRecurring` 定时器，触发 `callbackGroupData()`

---

## 7. Pub 端工作流程

```
init()
  └→ 创建 DataNode (DDS 参与者)
  └→ 创建 PubTableDefineWriter (变量定义广播)

createVars(vars)
  └→ 逐个变量: register_var() 到 VarStore, addMember() 到 BucketManager
  └→ finalize() 分配 arena
  └→ 按桶发布 PubTableDefine (DDS 广播变量定义)
  └→ 创建 DataTransferWriter (每桶一个)
  └→ schedulerDirty_ = true

setVarData(id, data, size)  [热路径]
  └→ VarStore::write() (seqlock 写入)

setVarDataBatch(items, count)  [批量热路径]
  └→ VarStore::write_batch()

processReceiveRegister()  [后台线程]
  └→ 收到 SubTableRegister
  └→ getOrAssignNodeBit() 分配订阅者位
  └→ 更新 freqSubs / recalcCurrentFreq()
  └→ schedulerDirty_ = true

processPublishTaskScheduler()  [后台线程]
  └→ schedulerDirty_ 检查
  └→ 构建 PublishGroupKey{bucket, freq} → 成员列表
  └→ ScheduleRecurring(publishGroupData, jitter, interval)

publishGroupData(bucketIdx, freq)  [定时器回调]
  └→ VarStore::read() 批量读取
  └→ 构建 Roaring64Map 位掩码
  └→ 序列化 TableDataTransfer
  └→ DDS Write 到 bucket_N Topic
```

---

## 8. Sub 端工作流程

```
init()
  └→ 创建 DataNode
  └→ 创建 PubTableDefineReader (接收变量定义)
  └→ 创建 SubTableRegisterWriter (发送订阅请求)

start()
  └→ 启动 processTableDefineThread (处理变量定义)
  └→ 启动 processDataTransferThread (处理数据传输)
  └→ 启动 callbackSchedulerThread (管理回调定时器)

subscribe(nodeName, items, callback)
  └→ 构建 SubTableRegister
  └→ DDS Write 发送订阅请求
  └→ 存储 callback 到 subscriptionCallbacks_
  └→ callbackDirty_ = true

processTableDefine()  [后台线程]
  └→ 收到 PubTableDefine
  └→ register_var() 到 VarStore
  └→ 创建 DataTransferReader (每桶一个)

processDataTransfer()  [后台线程]
  └→ 收到 TableDataTransfer
  └→ 反序列化 Roaring64Map
  └→ VarStore::write() 逐变量写入
  └→ 更新 varWriteStamps_[bucket] (timestampNs, writeCount)

processCallbackScheduler()  [后台线程]
  └→ callbackDirty_ 检查
  └→ 构建 CallbackGroupKey{bucket, freq} → 成员列表
  └→ ScheduleRecurring(callbackGroupData, jitter, interval)

callbackGroupData(bucketIdx, freqMs)  [定时器回调]
  └→ 检查 writeCount > 0 (有数据才回调)
  └→ VarStore::read() 批量读取
  └→ 构建 VarCallbackData 批量
  └→ 调用用户注册的 DataCallback
```

---

## 9. DDS 通信层

### 编译时抽象

通过 `dds_abstraction.h` 实现编译时切换：

```cpp
#ifdef USE_FASTDDS
    namespace DdsWrapper = FastddsWrapper;
#elif USE_TXDDS
    namespace DdsWrapper = TxddsWrapper;
#endif
```

业务代码统一使用 `DdsWrapper::DataNode`、`DdsWrapper::DDSTopicWriter<T>` 等别名。

### QoS 配置

| 配置项 | Pub | Sub |
|--------|-----|-----|
| 可靠性 | RELIABLE | RELIABLE |
| 持久性 | TRANSIENT_LOCAL | TRANSIENT_LOCAL |
| 历史 | KEEP_LAST | KEEP_LAST |
| 传输 | UDPV4 (多播) | UDPV4 |

通过 Builder 模式配置：`QoSBuilder().setReliabilityKind(...).setDurabilityKind(...)`

### 话题拓扑

| 话题 | 用途 | 方向 |
|------|------|------|
| `dsf/sys/var/tableDefine` | 变量定义广播 | Pub → Sub |
| `dsf/message/commandRequest/subTableRegister` | 订阅/取消请求 | Sub → Pub |
| `dsf/var/data/transfer/bucket_N` (N=0..19) | 数据传输 | Pub → Sub |

---

## 10. 性能优化要点

1. **VarStore 零锁读写**：seqlock + 双缓冲，读不阻塞写，写不阻塞读，无 mutex 开销
2. **无锁并发队列**：DDS 回调线程和处理线程之间用 moodycamel::ConcurrentQueue 解耦
3. **预分配缓冲区**：pub 端为每个分组预分配 flat buffer 和 mask buffer，避免热路径 malloc
4. **批量写入**：`setVarDataBatch()` 单次 seqlock 保护写入整批数据
5. **热路径 ID 缓存**：`getVarId()` 结果预缓存，热循环中不查找 map
6. **分桶隔离**：每桶独立 Topic/Writer/Reader，序列化互不阻塞
7. **jitter 调度对齐**：pub 按桶依次发送，sub 延后 5ms 接收，避免峰值拥塞
8. **Roaring bitmap**：用 Roaring64Map 压缩编码"哪些变量有数据"，序列化开销极小
9. **running flag**：回调分组级并发保护，上次回调未执行完则跳过本次，防止堆积
10. **dirty flag**：调度器仅在订阅信息变更时重建分组，平时 50ms 空转

---

## 11. 关键设计决策

| 决策 | 原因 |
|------|------|
| 20 个桶 | 平衡：桶太少→序列化峰值高；桶太多→Topic/Writer 过多。20 桶对应 5000 vars/桶 |
| seqlock 而非 mutex | 读多写少场景下 seqlock 比 mutex 延迟低得多，且无优先级反转风险 |
| CallbackGroupKey = {bucket, freq} | 与 Pub 的 PublishGroupKey 对齐，确保同桶同频的变量共享一个定时器 |
| jitter = bucketIdx * (freq/20) + 5ms | 按桶错开触发，sub 在 pub 发完后 5ms 读取，确保数据就绪 |
| VarStore 双缓冲 | 读端不需要任何同步原语，seqlock 检测写冲突后重试即可 |
| 写入后回调而不检查 writeCount 变化 | writeCount 只检查 == 0 跳过（未收到数据），收到后始终回调。避免因 DDS 延迟抖动导致误判丢包 |
