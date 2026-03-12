# VarStore 设计文档

## 概述

`VarStore` 是一个高性能、线程安全的变量值存储，专为**单写多读、高频写入**场景设计。
核心思路：所有变量数据存放在一块连续的对齐内存（arena）中，通过 seqlock 实现无锁读，通过 dirty 队列实现增量发布。

---

## 内存布局

```
arena (64-byte aligned)
┌──────────────────────────────────────────────────────────────┐
│  Slot[0]  (64-byte aligned)                                  │
│  ┌─────────────────┬────────────────────────────────────┐    │
│  │ seq (atomic u32)│ data[size]  (pad to 64-byte align) │    │
│  └─────────────────┴────────────────────────────────────┘    │
│  Slot[1]  (64-byte aligned)                                  │
│  ┌─────────────────┬────────────────────────────────────┐    │
│  │ seq (atomic u32)│ data[size]  (pad to 64-byte align) │    │
│  └─────────────────┴────────────────────────────────────┘    │
│  ...                                                         │
└──────────────────────────────────────────────────────────────┘

metas_[id] = { id, offset, size }
table_[hash] = id
```

- 每个 Slot 按 64 字节对齐（`alignas(64)`），避免 false sharing
- `align64(sizeof(Slot) + size)` 确保下一个 Slot 也对齐
- arena 只增不减（软删除不释放空间）

---

## 核心数据结构

| 结构 | 说明 |
|------|------|
| `VarMeta { id, offset, size }` | 变量元数据，`size=0` 表示软删除 |
| `Slot { seq, data[] }` | 64-byte 对齐，seq 用于 seqlock |
| `table_` | `hash → id` 映射，O(1) 查找 |
| `metas_` | `id → VarMeta`，按注册顺序排列 |
| `dirty_flags_[]` | per-id 原子 bool，标记是否有新写入 |
| `dirty_queue_` | 无锁队列，存放待发布的 dirty id |

---

## 并发模型

### 两类操作

```
Config 操作（register/unregister/finalize）
  └─ ConfigGuard: 加 config_mu_ 互斥锁
                  设置 reconfig_=true
                  等待 active_ops_ 归零
                  操作完成后 reconfig_=false

Op 操作（read/write/read_ptr/write_ptr）
  └─ OpGuard: 自旋等待 reconfig_=false
              active_ops_++
              双重检查 reconfig_（防竞态）
              操作完成后 active_ops_--
```

### 竞态防护（双重检查）

```
线程A (config_begin)          线程B (op_enter)
─────────────────────         ─────────────────────
reconfig_ = true              while(reconfig_) spin   ← 等待
while(active_ops_) spin       active_ops_++
                              if(reconfig_) {          ← 双重检查
                                  active_ops_--
                                  重新自旋
                              }
```

没有双重检查，线程B可能在 reconfig_ 设置前通过自旋，然后 config_begin 永远等不到 active_ops_ 归零。

---

## Seqlock 读写协议

```
写端 write()                    读端 read()
────────────────────            ────────────────────────────
seq++ (→奇数, acquire)          loop (max 10次):
memcpy(slot->data, src)           s1 = seq.load(acquire)
seq++ (→偶数, release)            if s1 & 1: continue  ← 写入中
                                  memcpy(dst, slot->data)
                                  if s1 == seq.load: return true
                                return false
```

- seq 奇数 = 写入中，读端跳过
- seq 偶数 = 数据稳定，读端校验前后 seq 一致则成功
- 最多重试 10 次，超出返回 false（极端高频写场景）

**约束：同一变量只能有一个写线程**，多写会破坏 seq 的奇偶语义。

---

## Dirty 追踪机制

```
write() 调用
    │
    ├─ dirty_flags_[id] == false ?
    │       │
    │       ├─ YES: exchange(true) + dirty_queue_.enqueue(id)
    │       └─ NO:  跳过（已在队列中，不重复入队）
    │
    └─ 数据写入 slot->data

for_each_dirty(fn, batch)
    │
    ├─ dirty_queue_.try_dequeue_bulk(ids, batch)
    ├─ dirty_flags_[id] = false  ← 在 OpGuard 内清除
    └─ fn(id)                    ← 在 OpGuard 外回调
```

- 同一变量多次写入只入队一次（幂等）
- 批量出队减少锁竞争
- 回调在 OpGuard 外执行，避免死锁

---

## 零拷贝接口

### read_ptr / ScopedPtr（零拷贝读）

```cpp
{
    auto ptr = varStore.read_ptr(id);  // op_enter()，active_ops_++
    if (ptr) {
        // ptr.get() 直接指向 slot->data，无 memcpy
        // 持有期间 config 操作会阻塞
    }
}  // 析构时 op_exit()，active_ops_--
```

**注意**：持有 ScopedPtr 期间不受 seqlock 保护，若写端并发写入可能读到撕裂数据。适合**读端确认写端已停止**的场景。

### write_ptr（零拷贝写）

```cpp
void *p = varStore.write_ptr(id);
// 直接写 *p，绕过 seqlock
// 线程安全由调用者保证（单写多读）
```

**风险**：绕过 seqlock，读端 `read()` 可能读到中间状态。仅适合调用者能保证写完整性的场景。

---

## 生命周期

```
register_var(hash, size)   ← 注册变量，分配 id 和 arena 空间
        │
finalize()                 ← 提交注册，分配/扩展 arena 内存
        │
write(id, data)            ← 高频写入
read(id, dst)              ← 读取
        │
unregister_var(hash)       ← 软删除（size=0，arena 不释放）
        │
~VarStore()                ← free(arena_), delete[] dirty_flags_
```

`finalize()` 必须在 `write/read` 之前调用，否则 `arena_` 为空，操作返回 false。

---

## 设计权衡

| 决策 | 优点 | 代价 |
|------|------|------|
| 软删除 | ID 稳定，无需重排 | arena 只增不减，有内存碎片 |
| seqlock | 读无锁，写不阻塞读 | 读端最多重试10次；多写不安全 |
| 64-byte 对齐 | 避免 false sharing，SIMD 友好 | 小变量浪费空间 |
| dirty 幂等入队 | 队列不膨胀 | 高频写同一变量只触发一次发布 |
| ConfigGuard 等待 active_ops_ | 无需 RCU/epoch，实现简单 | 持有 ScopedPtr 会阻塞 finalize |
| get_id() 无锁 | 高频查找零开销 | 与 unregister_var 并发可能返回已删除 id |

---

## 关键路径性能

**热路径（write → for_each_dirty）：**

```
setVarData(varName, data, size)
  └─ fast_hash(varName)           O(1)
  └─ varIndex_.find(hash)         O(1) shared_lock
  └─ varStore_.write(id, data)
       └─ OpGuard (原子操作)
       └─ seq++ / memcpy / seq++
       └─ dirty_flags_ exchange + enqueue (仅首次)

publishGroupData(bucket, freqMs)
  └─ for each varId in group:
       └─ varStore_.read(id, buf)  seqlock 读
       └─ msg.varData().emplace_back(buf, buf+size)  堆分配
  └─ DDS write
```

10w 变量 @ 200ms 的瓶颈在 `publishGroupData` 的 10w 次堆分配，而非 `write` 侧。
