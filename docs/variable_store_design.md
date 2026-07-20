# VarStore 设计文档

## 1. 概述

`VarStore` 是 OnDemand 系统的**核心变量值存储模块**，负责管理 10 万+ 变量的内存分配、读写、脏标记追踪。

设计目标：
- **一写多读**：同一变量同一时刻只有一个写端，但可以有多个读端并发读取
- **无锁读**：读操作不阻塞写操作，写操作不阻塞读操作
- **批量操作**：支持批量注册、批量写入、批量零拷贝读
- **动态扩容**：变量数据大小可以在运行时扩展
- **64 字节对齐**：避免 CPU cache line false sharing

---

## 2. 整体架构

```mermaid
graph TB
    subgraph 用户层
        PUB[OnDemandPub<br/>write / write_batch]
        SUB[OnDemandSub<br/>read / read_batch / read_zero_copy]
    end

    subgraph VarStore 存储引擎
        TABLE["table_<br/>hash → id"]
        METAS["metas_[]<br/>id → VarMeta{id, offset, size}"]
        ARENA["arena_<br/>64B 对齐连续内存<br/>[Slot0][Slot1]...[SlotN]"]
        DIRTY_FLAGS["dirty_flags_[]<br/>per-id 原子 bool"]
        DIRTY_QUEUE["dirty_queue_<br/>ConcurrentQueue 脏变量 id"]
    end

    subgraph 三层并发控制
        L1["Layer 1: Seqlock<br/>单 Slot atomic seq + 双缓冲<br/>读写互斥，无锁"]
        L2["Layer 2: OpGuard<br/>active_ops_ 共享计数<br/>允许多个并发读"]
        L3["Layer 3: ConfigGuard<br/>reconfig_ 独占标志<br/>等待所有读退出"]
    end

    PUB --> TABLE
    PUB --> METAS
    PUB --> ARENA
    SUB --> TABLE
    SUB --> METAS
    SUB --> ARENA
    ARENA --> L1
    L1 --> L2
    L2 --> L3
```

---

## 3. 内存布局

### 3.1 Slot 内部结构

```mermaid
block-beta
    columns 1
    block:header["Slot Header (64 bytes, alignas(64))"]
        columns 4
        SEQ["seq<br/>atomic u32<br/>Seqlock 计数器"]
        COMMIT["committed<br/>atomic u8<br/>当前可读 buf 索引"]
        VALID["valid_size[2]<br/>u32 × 2<br/>每个 buf 有效长度"]
        PAD["pad<br/>48 bytes"]
    end
    block:data["Data (双缓冲, 2 × payload_size bytes)"]
        columns 2
        BUF0["buf[0]<br/>payload_size bytes"]
        BUF1["buf[1]<br/>payload_size bytes"]
    end
```

| 字段 | 作用 | 为什么这样设计 |
|------|------|----------------|
| `seq` | Seqlock 计数器，奇数=写入中，偶数=稳定 | 无锁读的关键，读端通过检查 seq 判断数据是否有效 |
| `committed` | 指向当前可读的 buf (0 或 1) | 双缓冲切换，写端写 `1-committed`，写完后翻转 |
| `valid_size[2]` | 每个 buf 的实际有效数据长度 | 支持变长写入，读端知道要读多少字节 |
| `alignas(64)` | 64 字节对齐 | 一个 Slot 独占一个 cache line，避免 false sharing |

### 3.2 Arena 全局布局

```mermaid
block-beta
    columns 1
    block:arena["arena_ (std::byte*, 64 字节对齐分配)"]
        columns 3
        S0["Slot[0]<br/>offset=0<br/>size=s0"]
        S1["Slot[1]<br/>offset=align64(64+2×s0)<br/>size=s1"]
        SN["Slot[N-1]<br/>offset=...<br/>size=sn"]
    end
    block:meta["metas_[]"]
        columns 3
        M0["{id=0, offset=0, size=s0}"]
        M1["{id=1, offset=..., size=s1}"]
        MN["{id=N-1, offset=..., size=sn}"]
    end
    block:hash["table_"]
        H0["hash0 → 0"]
        H1["hash1 → 1"]
        HN["hashN → N-1"]
    end
```

---

## 4. 三层并发控制模型

这是 VarStore 最核心的设计。三层各司其职：

```mermaid
graph TB
    subgraph "Layer 3: ConfigGuard — 独占"
        CG["config_begin / config_end<br/>register / unregister / finalize / ensure_capacity<br/>等待所有 OpGuard 退出后才执行"]
    end
    subgraph "Layer 2: OpGuard — 共享"
        OG["op_enter / op_exit<br/>read / write / read_batch / read_zero_copy<br/>多个 OpGuard 可以同时存在"]
    end
    subgraph "Layer 1: Seqlock — 单 Slot"
        SL["atomic seq + 双缓冲<br/>读写互斥，无锁"]
    end

    CG -->|等待 active_ops_==0| OG
    OG -->|保护 arena 不被重分配| SL
```

### 4.1 Layer 1: Seqlock（单 Slot 读写互斥）

让读端和写端可以并发访问同一个 Slot，无需 mutex。

```mermaid
sequenceDiagram
    participant W as 写端 (write)
    participant S as Slot
    participant R as 读端 (read)

    W->>S: seq.fetch_add(1) → 奇数（写入中）
    W->>S: fence(release)
    W->>S: memcpy(buf[1-committed], src)
    W->>S: committed = 1-committed
    W->>S: seq.fetch_add(1) → 偶数（写入完成）

    R->>S: s1 = seq.load(acquire)
    alt s1 是奇数
        R-->>R: 写入中，yield 重试
    end
    R->>S: idx = committed.load(acquire)
    R->>S: memcpy(dst, buf[idx])
    R->>S: s2 = seq.load(acquire)
    alt s1 != s2
        R-->>R: 读取期间被改了，重试
    else s1 == s2
        R-->>R: 读取成功
    end
```

### 4.2 Layer 2: OpGuard（读操作保护）

防止读操作在执行期间，arena 被重新分配（`finalize` / `ensure_capacity`）。

```mermaid
graph LR
    A["op_enter()"] --> B["lock(reconfig_mu_)"]
    B --> C{"reconfig_ == true?"}
    C -->|是| D["wait(reconfig_cv_)<br/>等待 config 结束"]
    D --> C
    C -->|否| E["active_ops_++"]
    E --> F["unlock(reconfig_mu_)"]
    F --> G["执行读写操作"]
    G --> H["op_exit()<br/>active_ops_--"]
```

多个 OpGuard 可以同时存在（`active_ops_` 可以 > 1），它们共享读访问。

### 4.3 Layer 3: ConfigGuard（配置操作保护）

确保 register / unregister / finalize / ensure_capacity 执行时，没有读操作在进行。

```mermaid
graph LR
    A["config_begin()"] --> B["lock(config_mu_)"]
    B --> C["lock(reconfig_mu_)<br/>reconfig_ = true"]
    C --> D["unlock(reconfig_mu_)"]
    D --> E{"active_ops_ > 0?"}
    E -->|是| F["yield 等待"]
    F --> E
    E -->|否| G["*** 独占 arena ***<br/>执行配置操作"]
    G --> H["config_end()"]
    H --> I["lock(reconfig_mu_)<br/>reconfig_ = false"]
    I --> J["unlock(reconfig_mu_)<br/>notify_all(reconfig_cv_)"]
    J --> K["unlock(config_mu_)"]
```

### 4.4 为什么需要互斥锁？—— TOCTOU 竞态防护

`reconfig_` 是 atomic bool，但仅有它不够。**没有互斥锁的竞态：**

```mermaid
sequenceDiagram
    participant B as 线程B (op_enter)
    participant A as 线程A (config_begin)

    B->>B: s = reconfig_.load() → false
    Note over B: 读到 false，准备 active_ops_++
    A->>A: reconfig_ = true
    A->>A: while(active_ops_) spin ← 等 B
    B->>B: active_ops_++ ← B 进来了！
    Note over A: 永远等不到 active_ops_==0<br/>→ 死锁！
```

**有互斥锁：**

```mermaid
sequenceDiagram
    participant B as 线程B (op_enter)
    participant A as 线程A (config_begin)

    B->>B: lock(reconfig_mu_)
    Note over B: B 拿到锁
    A-->>A: lock(reconfig_mu_) ← 等 B 释放
    B->>B: while(!reconfig_) → true<br/>active_ops_++
    B->>B: unlock(reconfig_mu_)
    A->>A: lock(reconfig_mu_) ← 拿到锁
    A->>A: reconfig_ = true
    A->>A: while(active_ops_) spin<br/>等 B op_exit
    Note over A: B 已经 ++ 了，等它退出即可
```

互斥锁保证"检查 `reconfig_`"和"`active_ops_++`"是原子的，A 不可能在中间插入。

### 4.5 完整的锁交互时序

```mermaid
sequenceDiagram
    participant W as 写线程 (write_batch)
    participant C as 扩容线程 (ensure_capacity_batch)
    participant R as 读线程 (read)

    Note over C: config_begin()<br/>lock(config_mu_)<br/>reconfig_ = true<br/>等 active_ops_==0

    W->>W: op_enter():<br/>lock(reconfig_mu_)<br/>while(reconfig_) → TRUE!<br/>wait(reconfig_cv_) ← 阻塞
    R->>R: op_enter():<br/>lock(reconfig_mu_)<br/>while(reconfig_) → TRUE!<br/>wait(reconfig_cv_) ← 阻塞

    Note over C: 扩容中...<br/>realloc arena<br/>copy data

    Note over C: config_end()<br/>reconfig_ = false<br/>notify_all(reconfig_cv_)<br/>unlock(config_mu_)

    W->>W: reconfig_ = false<br/>active_ops_++ ← 醒了
    R->>R: reconfig_ = false<br/>active_ops_++ ← 醒了

    W->>W: 写入数据...
    R->>R: 读取数据...

    W->>W: op_exit():<br/>active_ops_--
    R->>R: op_exit():<br/>active_ops_--
```

---

## 5. 变量生命周期

```mermaid
stateDiagram-v2
    [*] --> UNALLOC: register_var(hash, 0)

    UNALLOC --> READY: ensure_capacity<br/>分配内存, size > 0

    READY --> READY: write() / read()

    READY --> UNALLOC: unregister_var<br/>软删除, size = 0

    UNALLOC --> [*]: ~VarStore()
    READY --> [*]: ~VarStore()
```

- **UNALLOC** (`size=0`)：已注册但未分配内存，`write()` 返回 `NOT_READY`，`read()` 返回 `nullptr`
- **READY** (`size>0`)：内存已分配，可正常读写
- **软删除**：`unregister_var` 将 `size` 置 0，不释放 arena（ID 不变，不重排）

---

## 6. 写入路径

### 6.1 单变量写入：`write(id, src, actual_size)`

```mermaid
flowchart TD
    A["write(id, src, actual_size)"] --> B{"src == null || size == 0?"}
    B -->|是| C["return FAILED"]
    B -->|否| D["OpGuard 加锁"]
    D --> E{"arena 有效 && id < var_count?"}
    E -->|否| F["return FAILED"]
    E -->|G{"metas_[id].size == 0?"}
    G -->|是| H["return NOT_READY"]
    G -->|否| I{"actual_size <= size?"}
    I -->|是| J["seqlock 写入 buf[1-committed]<br/>dirty_flags_[id] = true<br/>dirty_queue_.enqueue(id)"]
    J --> K["return SUCCESS"]
    I -->|否| L["OpGuard 解锁"]
    L --> M["ensure_capacity(id, actual_size)<br/>ConfigGuard: 扩容 arena"]
    M --> N["continue → 回到循环顶部"]
    N --> D
```

### 6.2 批量写入：`write_batch(ids, datas, sizes, count)`

```mermaid
flowchart TD
    A["write_batch(ids, datas, sizes, count)"] --> B["Step 1: relaxed 扫描（不加锁）"]
    B --> C["遍历所有 item:<br/>size==0 → 标记 not_ready<br/>sizes[i] > size → 加入 needExpands"]
    C --> D["Step 2: 批量扩容（ConfigGuard）"]
    D --> E["for each (id, reqSize) in needExpands:<br/>ensure_capacity(id, reqSize)"]
    E --> F["Step 3: 单次 OpGuard 写入"]
    F --> G["遍历所有 item:<br/>seqlock 写入 buf[1-committed]<br/>dirty_flags_[id] = true"]
    G --> H["return SUCCESS / NOT_READY"]
```

**Step 1 为什么不加锁？**
- `metas_[id].size` 是 `uint32_t`，relaxed load 是原子的
- 最坏情况读到旧值，Step 3 在 OpGuard 内兜底
- 好处：不被 ConfigGuard 阻塞，30 万变量扫描瞬间完成

---

## 7. 读取路径

### 7.1 带 memcpy 的安全读：`read(id, dst)`

```mermaid
flowchart TD
    A["read(id, dst)"] --> B["OpGuard 加锁"]
    B --> C["s1 = seq.load(acquire)"]
    C --> D{"s1 是奇数？"}
    D -->|是| E["写入中，yield"]
    E --> C
    D -->|否| F["idx = committed.load(acquire)<br/>memcpy(dst, buf[idx])"]
    F --> G["s2 = seq.load(acquire)"]
    G --> H{"s1 == s2？"}
    H -->|否| I["读取期间被改了，重试"]
    I --> C
    H -->|是| J["return true"]
```

### 7.2 零拷贝读：`read_zero_copy(id)`

```mermaid
sequenceDiagram
    participant U as 用户代码
    participant H as ZeroCopyReadHandle
    participant V as VarStore

    U->>H: 构造 handle(store, id)
    H->>V: op_enter() → active_ops_++
    H->>H: seqlock 验证
    H->>H: ptr_ = buf[idx]（直接指向 arena）
    H-->>U: 返回句柄

    U->>U: 直接读 handle.ptr()<br/>无 memcpy

    U->>H: handle 析构
    H->>V: op_exit() → active_ops_--
```

**注意**：持有 `ZeroCopyReadHandle` 期间，`config_begin` 会被阻塞（因为 `active_ops_ > 0`）。用完立即释放。

---

## 8. Dirty 追踪机制

用于 OnDemand 的增量发布：只发布有新数据的变量。

```mermaid
flowchart LR
    A["write() 成功"] --> B{"dirty_flags_[id]<br/>exchange(true)"}
    B -->|原来 false| C["dirty_queue_.enqueue(id)<br/>首次脏，入队"]
    B -->|原来 true| D["已在队列，跳过<br/>幂等"]

    E["for_each_dirty(fn)"] --> F["OpGuard 加锁"]
    F --> G["dirty_queue_.try_dequeue_bulk<br/>批量出队"]
    G --> H["dirty_flags_[id] = false"]
    H --> I["OpGuard 解锁"]
    I --> J["fn(id) 回调<br/>锁外执行"]
```

**幂等入队**：同一个变量多次写入只入队一次，队列不膨胀。

---

## 9. 动态扩容

```mermaid
flowchart TD
    A["ensure_capacity(id, required)"] --> B["ConfigGuard 加锁"]
    B --> C{"required <= old_size?"}
    C -->|是| D["已够大，return true"]
    C -->|否| E["计算新 arena 大小"]
    E --> F["aligned_alloc(64, new_size)"]
    F --> G["逐个拷贝 Slot 到新 arena"]
    G --> H["free(旧 arena)"]
    H --> I["更新 metas_[id].size<br/>更新所有 offset"]
    I --> J["ConfigGuard 解锁"]
```

批量扩容 `ensure_capacity_batch` 逻辑相同，一次 ConfigGuard 处理所有变量，只做一次 arena 重分配。

---

## 10. 设计权衡总结

| 决策 | 优点 | 代价 |
|------|------|------|
| Seqlock 无锁读 | 读不阻塞写，写不阻塞读 | 读端可能重试；只能单写 |
| 双缓冲 | 写端不阻塞读端 | 每个变量 2 倍内存 |
| 64 字节对齐 | 无 false sharing，SIMD 友好 | 小变量浪费空间（最小 128B/Slot） |
| 软删除 (size=0) | ID 稳定，无需重排 | arena 只增不减 |
| ConfigGuard + OpGuard | 配置和读写互斥，实现简单 | ConfigGuard 期间所有读写阻塞 |
| 互斥锁 + 条件变量 | 防止 TOCTOU 竞态 | 每次 op_enter 有锁开销（很小） |
| relaxed load 扫描 | 批量写不被 ConfigGuard 阻塞 | 可能读到旧值，需兜底 |
| dirty 幂等入队 | 队列不膨胀 | exchange 有原子开销 |
