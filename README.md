1. 项目定位
基于 DDS（数据分发服务）的大规模变量按需发布订阅框架。支持 10 万级变量 的高效发布与订阅，通过分桶隔离、时间轮调度、无锁存储等机制，实现低延迟、高吞吐的数据传输。
核心场景：工业控制系统、实时仿真等需要大量变量按不同频率发布/订阅的场景。

2. 解决什么问题
传统 DDS 发布订阅模式中，所有变量共享同一个定时器、同一个数据通道。当变量数量达到 10 万级别时：
● 定时器瓶颈：单定时器无法为不同桶的变量提供差异化调度
● 数据通道竞争：所有变量挤在同一个 Topic 上，序列化/反序列化开销大
● 无法按需订阅：订阅端只能全量接收，无法指定"只订阅某些变量、以特定频率"
onDemand 的解决方案：hash 分桶 + 独立通道 + 按频率分组调度

3. 整体架构
┌─────────────────────────────────────────────────┐
│                  用户业务层                       │
│   pub.setVarData() / sub.subscribe(callback)    │
├─────────────────────────────────────────────────┤
│              OnDemandPub / OnDemandSub           │
│  pub: createVars/deleteVars → publishGroupData  │
│  sub: subscribe → processDataTransfer → callback│
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
分层说明
层	职责	关键文件
业务层	变量注册/删除、订阅、回调分组	on_demand_pub.cc, on_demand_sub.cc
存储层	无锁双缓冲变量存储	variable_store.h
调度层	时间轮定时器 + 异步执行	timer_scheduler.h, timer_wheel.h
通信层	DDS 封装，编译时切换底层实现	fastdds_node.h, dds_abstraction.h

4. 分桶机制
为什么分桶
将 10 万变量按 hash 分成 20 个桶（ONDEMAND_BUCKET_SIZE = 20），每个桶：
● 独立的 DDS Topic（dsf/var/data/transfer/bucket_N）
● 独立的 DataWriter（pub）/ DataReader（sub）
● 独立的定时器和调度周期
分桶逻辑
bucketIndex = hash(varName) % 20
每桶约 5000 个变量。pub 端逐桶发送数据，避免所有变量同时序列化造成的 CPU 尖峰。
调度对齐（Jitter）
pub 和 sub 的定时器触发时机通过 jitter 对齐：
staggerStep = max(freqMs / 20, 5ms)
jitter = bucketIdx * staggerStep
首次触发 = freqMs + jitter
● pub 的 bucket0 最先触发，bucket1 延后 staggerStep，依次错开
● sub 的对应 bucket 使用相同 jitter 公式，确保 sub 读到的是当周期数据

5. VarStore — 无锁变量存储
基于 seqlock + 双缓冲 的无锁并发存储，支持一写多读。
内存布局
Arena: [Slot0][Slot1][Slot2]...  (64 字节对齐分配)

Slot 结构 (header 固定 64B):
┌──────┬──────────┬──────────────┬────────┐
│ seq  │committed │ valid_size[2]│ pad    │
│ u32  │  u8      │   u32×2      │ 48B    │
├──────┴──────────┴──────────────┴────────┤
│  data[0]: size bytes                    │
│  data[1]: size bytes                    │
└─────────────────────────────────────────┘
● seq：原子计数器，奇数表示"正在写入"，偶数表示"稳定可读"
● committed：0 或 1，指向当前可读的缓冲区
● valid_size[2]：每个缓冲区的实际有效数据长度
写入流程（精确内存序）
seq.fetch_add(1, relaxed) + fence(release)  // 写开始，阻止 memcpy 上移
memcpy(data[1-committed], src, size)
valid_size[write_idx] = actual_size
committed.store(write_idx, release)         // 翻转可读缓冲区
seq.fetch_add(1, release)                   // 写结束，让读端可见
读取流程
for (;;) {
    s1 = seq.load(acquire)
    if (s1 & 1) yield; continue;   // 正在写入，自旋等待
    idx = committed.load(acquire)
    fence(acquire)
    // 读取 data[idx]
    if (s1 == seq.load(acquire)) break;  // seq 未变，读取有效
}
无重试上限，实际写入竞争极少时通常 1 次即可完成。
软删除（unregister_var）
unregister_var(hash) 将 metas_[id].size 置 0，不释放 arena 内存（软删除）。读端检测到 size == 0 时返回 nullptr，发布端跳过该变量。
主要接口
接口	说明
register_var(hash, size)	注册变量，返回 id
unregister_var(hash)	软删除，标记 size=0
finalize()	一次性分配/扩容 aligned arena，支持动态增量扩展
write(id, src)	单变量写入，seqlock 保护
write(id, src, actual_size)	支持变长写入，自动扩容
write_batch(ids, datas, sizes, count)	批量写入，单次 OpGuard 覆盖所有变量
read(id, dst)	带 memcpy 的安全读
read_zero_copy(id)	返回 ZeroCopyReadHandle，直接指向 arena，无 memcpy
read_batch(ids, count, fn)	批量零拷贝读，单次 op_enter 覆盖所有 N 个变量，fn(i, ptr, size) 回调
for_each_dirty(fn)	遍历 dirty 队列，获取有更新的变量 id
OpGuard / ConfigGuard
● OpGuard：读操作保护，op_enter/op_exit 递增/递减 active_ops_，防止 finalize() 在读取期间重分配 arena
● ConfigGuard：写配置保护，config_begin/config_end 等待 active_ops_==0 后独占执行

6. 时间轮调度
TimerWheel（底层）
8 级层次化时间轮，每级 256 个槽。总覆盖范围：256^8 ticks。事件按延迟大小自动选择层级，短延迟在低层级（精确），长延迟在高层级。
TimerScheduler（封装层）
方法	语义
Schedule(cb, delay)	单次执行
ScheduleRecurring(cb, delay, interval)	周期执行（fixed-rate：先重排再回调）
Post(cb)	通过线程池立即异步执行
Cancel(timer)	取消定时器
定时器线程每 1ms tick 一次。回调通过 ThreadPool 异步执行，不阻塞定时器循环。
Pub/Sub 中的使用
● Pub：每个 (bucketIdx, freqMs) 组合创建一个 ScheduleRecurring 定时器，触发 publishGroupData()
● Sub：每个 (bucketIdx, freqMs) 组合创建一个 ScheduleRecurring 定时器，触发 callbackGroupData()

7. Pub 端工作流程
init()
  └→ 创建 DataNode (DDS 参与者，注册 ParticipantListener)
  └→ 创建 PubTableDefineWriter (变量定义广播)
  └→ 创建 SubTableRegisterReader (接收订阅请求)

start()
  └→ 创建 TimerScheduler (1ms tick, 8线程池)
  └→ 启动 registerProcessThread (处理订阅注册请求)
  └→ 启动 freqChangeCbThread (派发频率变化回调)
  └→ 启动 publishSchedulerThread (管理发布定时器)

createVars(vars)
  └→ 逐个变量: register_var() 到 VarStore, AddMember() 到 BucketManager
  └→ 释放写锁后 finalize() 分配 arena
  └→ createDataTransferWriter() (每桶一个 Writer)
  └→ 按桶发布 PubTableDefine (每桶间隔 50ms，避免 DDS 拥塞)
  └→ schedulerDirty_ = true

deleteVars(varNames)
  └→ 持写锁: unregister_var() + varIndex_.erase() + BucketManager.RemoveMember()
  └→ 记录 affectedBuckets (只重发受影响的桶)
  └→ schedulerDirty_ = true
  └→ 仅对 affectedBuckets 重发 PubTableDefine (含空表通知)

setVarData(varName, data, size)  [按名写入]
  └→ VarStore::write()

setVarData(varId, data, size)  [按 id 写入，热路径]
  └→ VarStore::write()

setVarDataBatch(items, count)  [批量热路径]
  └→ VarStore::write_batch() (单次 OpGuard 覆盖所有变量)

processReceiveRegister()  [后台线程]
  └→ 消费 pubTableDefRegisterQueue_
  └→ SUB_TABLE_REGISTER: handleSubscribe()
       └→ getOrAssignNodeBit() 分配订阅者位 (最多 64 个节点)
       └→ 更新 freqSubs / recalcCurrentFreq()
       └→ 若有变量未创建则指数退避重试
       └→ schedulerDirty_ = true
       └→ 频率变化入 freqChangeQueue_
  └→ SUB_TABLE_UNREGISTER: handleUnsubscribe()
       └→ 从 freqSubs 移除该节点订阅位
       └→ recalcCurrentFreq()
       └→ schedulerDirty_ = true

processPublishTaskScheduler()  [后台线程, 50ms 扫描]
  └→ schedulerDirty_ 检查 (false 则跳过)
  └→ 从 varIndex_ 构建 desired: PublishGroupKey{bucket, freq} → GroupVarInfo[]
  └→ 按 varHash 升序排序 (与 Roaring64Map 迭代顺序对齐)
  └→ 取消不再需要的分组定时器
  └→ 为新分组创建 ScheduleRecurring 定时器
  └→ 预计算每组的 Roaring64Map 并序列化到 groupMaskBufs_

publishGroupData(bucketIdx, freq)  [定时器回调]
  └→ running flag 防并发重入
  └→ read_batch(ids, n, fn): 单次 op_enter 批量读取所有变量
  └→ 跳过 size==0 的软删除变量，记录 skippedCount
  └→ 无跳过: 直接用 groupMaskBufs_ 预计算 mask (热路径零重建)
  └→ 有跳过: 重建 actualMask (backfill 已发变量 hash)
  └→ 序列化 TableDataTransfer + 时间戳
  └→ DDS Write 到 bucket_N Topic

processFreqChangeCallback()  [后台线程]
  └→ 消费 freqChangeQueue_
  └→ 调用用户注册的 FreqChangeCallback(varName, newFreqMs)
  └→ 回调未注册时重新入队，避免丢失事件

onParticipantDiscovery()  [DDS 回调]
  └→ 仅处理 REMOVED / DROPPED 状态
  └→ cleanupParticipantSubscriptions(participantName)
       └→ 通过 nodeSlotMap_ 找到该节点的 bit 位
       └→ forceUnsubscribeNode(nodeMask): 强制清除所有变量的该节点订阅
       └→ schedulerDirty_ = true
       └→ 频率变化入 freqChangeQueue_

8. Sub 端工作流程
init()
  └→ 创建 DataNode (注册 ParticipantListener)
  └→ 创建 PubTableDefineReader (接收变量定义)
  └→ 创建 SubTableRegisterWriter (发送订阅请求)

start()
  └→ 启动 processTableDefineThread
  └→ 启动 4 个 processDataTransferThread (并行处理数据)
  └→ 创建 TimerScheduler (1ms tick, 4线程池)
  └→ 启动 callbackSchedulerThread

subscribe(nodeName, items, callback)
  └→ 构建 SubTableRegister (varFreqs 使用 META 全名: nodeName_varName)
  └→ DDS Write 发送订阅请求到 pub
  └→ 存储 SubCallbackInfo{freqMs, callback, varName} 到 subscriptionCallbacks_
  └→ callbackDirty_ = true

processTableDefine()  [后台线程]
  └→ 收到 PubTableDefine
  └→ 解析 bucket id (格式: bucket_N)
  └→ 差分删除: 找出 varIndex_ 中属于该 bucket 但本次 TableDefine 没有的变量
       └→ unregister_var() + varIndex_.erase() + subscriptionCallbacks_.erase()
       └→ totalReceived_--
  └→ 注册新增变量: register_var() + varIndex_.emplace()
  └→ finalize() 扩容 arena
  └→ 触发 tableDefineCb_ (外部可感知变量定义变化)
  └→ callbackDirty_ = true
  └→ 增量创建 DataTransferReader (仅新增 bucket)

processDataTransfer()  [4 个后台线程并行]
  └→ 收到 TableDataTransfer
  └→ 反序列化 Roaring64Map mask (确定哪些变量有数据)
  └→ 按 mask 迭代顺序逐变量 VarStore::write()
  └→ 更新 varWriteStamps_[bucket].{timestampNs, writeCount, blobType}

processCallbackScheduler()  [后台线程, 50ms 扫描]
  └→ callbackDirty_ 检查 (false 则跳过)
  └→ 从 subscriptionCallbacks_ ∩ varIndex_ 构建 desired 分组
  └→ 取消不再需要的分组定时器
  └→ 为新分组创建 ScheduleRecurring 定时器

callbackGroupData(bucketIdx, freqMs)  [定时器回调]
  └→ running flag 防并发重入
  └→ 读取 varWriteStamps_[bucket].writeCount，为 0 则跳过 (未收到数据)
  └→ 快照 timestampNs / blobType (前后 writeCount 一致性校验，最多重试 3 次)
  └→ 逐变量 read_zero_copy()，拷贝到 dataBuf (逐个释放 handle 避免与 finalize 死锁)
  └→ 构建 VarCallbackData[] 批量
  └→ 调用用户注册的 DataCallback

onParticipantDiscovery()  [DDS 回调]
  └→ 仅处理 REMOVED / DROPPED 状态
  └→ 清除该 pub 节点的所有变量: varIndex_.erase() + unregister_var()
  └→ 清除对应 subscriptionCallbacks_ 条目
  └→ totalReceived_ 递减
  └→ callbackDirty_ = true (触发定时器取消)
  └→ pub 重启后 sub 需手动重新调用 subscribe()

9. DDS 通信层
编译时抽象
通过 dds_abstraction.h 实现编译时切换：
#ifdef USE_FASTDDS
    namespace DdsWrapper = FastddsWrapper;
#elif USE_TXDDS
    namespace DdsWrapper = TxddsWrapper;
#endif
业务代码统一使用 DdsWrapper::DataNode、DdsWrapper::DDSTopicWriter<T> 等别名。
QoS 配置
配置项	Pub	Sub
可靠性	RELIABLE	RELIABLE
持久性	TRANSIENT_LOCAL	TRANSIENT_LOCAL
历史	KEEP_LAST	KEEP_LAST
传输	UDPV4 (多播)	UDPV4
通过 Builder 模式配置：QoSBuilder().setReliabilityKind(...).setDurabilityKind(...)
话题拓扑
话题	用途	方向
dsf/sys/var/tableDefine	变量定义广播	Pub → Sub
dsf/message/commandRequest/subTableRegister	订阅/取消请求	Sub → Pub
dsf/var/data/transfer/bucket_N (N=0..19)	数据传输	Pub → Sub

10. 性能优化要点
1. VarStore 零锁读写：seqlock + 双缓冲，读不阻塞写，写不阻塞读，无 mutex 开销
2. read_batch 批量读：publishGroupData 热路径用 read_batch，单次 op_enter 覆盖 N 个变量，消除 N×OpGuard 开销
3. 预计算 mask：processPublishTaskScheduler 重建分组时预序列化 Roaring64Map 到 groupMaskBufs_，无跳过时热路径直接复用，避免每次重建
4. 无锁并发队列：DDS 回调线程和处理线程之间用 moodycamel::ConcurrentQueue 解耦
5. 批量写入：setVarDataBatch() 单次 OpGuard 覆盖整批数据，热循环首选
6. 热路径 ID 缓存：getVarId() 结果预缓存，热循环中不查找 map
7. 分桶隔离：每桶独立 Topic/Writer/Reader，序列化互不阻塞；4 个 DataTransfer 线程并行处理
8. jitter 调度对齐：pub 按桶依次发送，sub 使用相同 jitter 公式，避免峰值拥塞
9. Roaring bitmap：用 Roaring64Map 压缩编码"哪些变量有数据"，序列化开销极小
10. running flag：回调/发布分组级并发保护，上次未执行完则跳过本次，防止堆积
11. dirty flag：调度器仅在订阅信息变更时重建分组，平时 50ms 空转

11. 关键设计决策
决策	原因
20 个桶	平衡：桶太少→序列化峰值高；桶太多→Topic/Writer 过多。20 桶对应约 5000 vars/桶
seqlock 而非 mutex	读多写少场景下 seqlock 比 mutex 延迟低，且无优先级反转风险
软删除（size=0）而非释放 arena	arena 重分配需要 ConfigGuard 等待所有读操作完成，代价高；软删除只需标记，读端自动跳过
deleteVars 只重发 affectedBuckets	若重发所有 20 个桶的 TableDefine，sub 的差分删除会误清其他桶的变量
sub 差分删除在 processTableDefine 中	TableDefine 是 pub 端的权威变量列表，收到即可做 diff，无需额外协议
pub 掉线后 sub 清除 subscriptionCallbacks_	pub 重启后变量定义可能变化，sub 应由用户显式重新 subscribe，避免用旧回调接收新数据
nodeSlotMap_ 最多 64 个订阅节点	用 uint64_t bitmask 表示订阅者集合，位操作 O(1)；超过 64 个节点时拒绝新订阅
CallbackGroupKey = {bucket, freq}	与 Pub 的 PublishGroupKey 对齐，确保同桶同频的变量共享一个定时器
callbackGroupData 逐个释放 ZeroCopyReadHandle	同时持有多个 handle 会让 active_ops 无法归零，导致与 finalize() 的 ConfigGuard 死锁
FreqChangeCallback 通过独立线程派发	与注册处理线程解耦，避免回调阻塞订阅处理；回调未注册时重新入队不丢事件
