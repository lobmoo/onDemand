# OnDemand 通信协议文档

> 本文档描述 OnDemand 按需发布-订阅系统的完整通信协议，供第三方对接方实现兼容客户端参考。

---

## 目录

1. [架构概览](#1-架构概览)
2. [DDS 基础配置](#2-dds-基础配置)
3. [消息类型定义（IDL）](#3-消息类型定义idl)
4. [通信流程总览](#4-通信流程总览)
5. [阶段一：变量定义广播（Pub → Sub）](#5-阶段一变量定义广播pub--sub)
6. [阶段二：订阅注册（Sub → Pub）](#6-阶段二订阅注册sub--pub)
7. [阶段三：数据传输（Pub → Sub）](#7-阶段三数据传输pub--sub)
8. [阶段四：取消订阅（Sub → Pub）](#8-阶段四取消订阅sub--pub)
9. [数据存储与读取](#9-数据存储与读取)
10. [频率协商机制](#10-频率协商机制)
11. [哈希函数规范](#11-哈希函数规范)
12. [断线重连机制](#12-断线重连机制)
13. [对接方实现 Checklist](#13-对接方实现-checklist)

---

## 1. 架构概览

OnDemand 是一个按需发布-订阅系统，核心特点：**订阅者指定每个变量的更新频率，发布者按所有订阅者的最小频率发送数据**。

```
┌─────────────┐                          ┌─────────────┐
│  Publisher   │                          │  Subscriber  │
│              │  ① PubTableDefine         │              │
│  registerVars│ ───────────────────────→ │  注册变量定义  │
│              │                          │              │
│              │  ② SubTableRegister       │              │
│  处理订阅     │ ←─────────────────────── │  subscribe() │
│              │                          │              │
│              │  ③ TableDataTransfer      │              │
│  定时发布数据  │ ───────────────────────→ │  回调用户函数  │
│              │                          │              │
│              │  ④ SubTableRegister       │              │
│  处理退订     │ ←─────────────────────── │  unsubscribe │
└─────────────┘                          └─────────────┘
```

### 关键设计

- **20 个 Bucket**：变量按 `hash % 20` 分桶，每个桶对应独立的 DDS Topic，实现并行传输
- **懒创建**：`registerVars` 仅注册元数据，不分配内存；当订阅者首次订阅时才真正创建变量（`createVars`）
- **频率协商**：每个变量维护 `freqSubs` 列表，`currentFreq` = 所有订阅者请求频率的最小值
- **Roaring64Map**：用位图标识本次传输包含哪些变量，`varData` 数组与位图迭代顺序一一对应

---

## 2. DDS 基础配置

### Domain ID

```
DOMAIN_ID = 66
```

### QoS 设置

| 参数 | 值 |
|------|-----|
| 传输协议 | UDPv4 |
| 多播地址 | 239.255.0.1:7400 / 7401 |
| 可靠性（数据传输） | BEST_EFFORT |
| 可靠性（订阅注册） | RELIABLE |
| 持久性（订阅注册） | TRANSIENT_LOCAL |
| 历史深度（订阅注册） | 32 |

---

## 3. 消息类型定义（IDL）

### 3.1 基础类型

```idl
// 二进制块（最大 2048 字节建议值）
typedef sequence<octet, 2048> DT_BLOB;

// 时间戳
struct Timespec {
    long tv_sec;        // 秒（Unix 时间戳）
    unsigned long tv_nsec; // 纳秒余数
};

// 名值对
struct NamedValue {
    string name;
    string value;
};
```

### 3.2 变量定义（Pub → Sub）

```idl
module DSF::Var {

    // 变量定义
    struct Define {
        string name;            // 变量名（不含节点前缀）
        int32 size;             // 变量大小（字节）
        string modelName;       // 所属模型名
        string modelVersion;    // 模型版本
        string description;     // 描述
        string nodeName;        // 所属发布节点名
        boolean isReadonly;     // 是否只读
        VarPublishMask publishMask; // 发布能力掩码
    };

    // 表中的单个变量定义
    struct PubTableVarDefine {
        uint16 id;              // 变量在表中的 ID
        VarRequest var;         // 变量定义请求
    };

    // 表定义（一次广播包含一个 bucket 的所有变量）
    struct PubTableDefine {
        @key string name;       // 表名，格式 "bucket_N"（N=0~19）
        @key string nodeName;   // 发布节点名
        string description;
        sequence<PubTableVarDefine> varDefines; // 变量定义列表
    };
}
```

### 3.3 订阅注册（Sub → Pub）

```idl
module DSF::Message {

    enum MSGTYPE {
        CONTROL,
        EVENT,
        ALARM,
        SUB_TABLE_REGISTER,     // 订阅
        SUB_TABLE_UNREGISTER    // 取消订阅
    };

    struct SubTableRegister {
        MSGTYPE msgType;            // SUB_TABLE_REGISTER 或 SUB_TABLE_UNREGISTER
        sequence<NamedValue> varFreqs;  // 变量名 + 频率（ms），name=完整变量名, value=频率字符串
        string tableName;           // 表名 "bucket_N"
        string nodeName;            // 订阅者节点名
        string user;
    };
}
```

**varFreqs 字段说明：**
- `name` = 完整变量名，格式为 `"{pubNodeName}_{varName}"`（例如 `"pubNode0_var1"`）
- `value` = 更新频率，单位毫秒，字符串形式（例如 `"100"` 表示 100ms）

### 3.4 数据传输（Pub → Sub）

```idl
module DSF::Var {

    enum BLOB_TYPE {
        UNKNOWN, NGVS, YAS, STRUCTS, TX
    };

    // 批量数据传输消息
    struct TableDataTransfer {
        DT_BLOB mask;               // Roaring64Map 序列化后的字节流
        Timespec timestamp;         // 发布时间戳
        sequence<DT_BLOB> varData;  // 变量数据数组，与 mask 位图迭代顺序一一对应
        BLOB_TYPE blobType;         // 数据序列化方式（默认 STRUCTS）
    };
}
```

---

## 4. 通信流程总览

```
时序图：

  Publisher                                    Subscriber
     │                                             │
     │  ──① PubTableDefine (per bucket)──→         │  变量定义广播
     │     topic: dsf/sys/var/tableDefine           │
     │                                             │
     │  ←──② SubTableRegister (REGISTER)──         │  订阅请求
     │     topic: dsf/message/commandRequest/       │
     │            subTableRegister                  │
     │                                             │
     │  ──③ TableDataTransfer (per bucket)──→      │  数据传输
     │     topic: dsf/var/data/transfer/bucket_N    │
     │                                             │
     │  ←──④ SubTableRegister (UNREGISTER)──       │  取消订阅
     │     topic: dsf/message/commandRequest/       │
     │            subTableRegister                  │
     │                                             │
```

### DDS Topic 列表

| Topic | 方向 | 消息类型 | 用途 |
|-------|------|---------|------|
| `dsf/sys/var/tableDefine` | Pub → Sub | `PubTableDefine` | 广播变量定义（每个 bucket 一条） |
| `dsf/message/commandRequest/subTableRegister` | Sub → Pub | `SubTableRegister` | 订阅 / 取消订阅请求 |
| `dsf/var/data/transfer/bucket_0` ~ `bucket_19` | Pub → Sub | `TableDataTransfer` | 批量变量数据（20 个 bucket 并行） |

---

## 5. 阶段一：变量定义广播（Pub → Sub）

### 5.1 触发时机

发布者调用 `registerVars(defines)` 或 `createVars(defines)` 后立即广播。

### 5.2 广播内容

每个 bucket 生成一条 `PubTableDefine` 消息：

```
PubTableDefine {
    name:       "bucket_3"              // bucket 名称
    nodeName:   "pubNode0"              // 发布节点名
    description: "onDemandPub TableDefine"
    varDefines: [                       // 该 bucket 中的所有变量
        { id: 0, var: { name: "var0", size: 4, nodeName: "pubNode0", ... } },
        { id: 1, var: { name: "var1", size: 8, nodeName: "pubNode0", ... } },
        ...
    ]
}
```

### 5.3 变量名与 Hash

变量在系统内部以 **完整变量名** 标识，格式为 `"{pubNodeName}_{varName}"`。

例如发布节点名为 `"pubNode0"`，变量名为 `"temperature"`，则：
- 完整变量名 = `"pubNode0_temperature"`
- Hash = `fast_hash("pubNode0_temperature")` （FNV-1a 64 位）
- Bucket = `hash % 20`

### 5.4 订阅者处理

订阅者收到 `PubTableDefine` 后：

1. 解析表名提取 bucket ID
2. 对每个变量计算 hash，在本地 `varStore_` 中注册（分配存储槽位）
3. 将变量信息存入本地 `varIndex_`，等待后续订阅请求

---

## 6. 阶段二：订阅注册（Sub → Pub）

### 6.1 订阅请求构造

订阅者调用 `subscribe(nodeName, items, callback)` 时：

```
SubTableRegister {
    msgType:    SUB_TABLE_REGISTER
    nodeName:   "subNode0"              // 订阅者节点名
    tableName:  "bucket_3"              // 任意一个 bucket 名（用于路由）
    varFreqs: [
        { name: "pubNode0_temperature", value: "100" },  // 100ms 更新一次
        { name: "pubNode0_pressure",    value: "200" },  // 200ms 更新一次
        { name: "pubNode0_flow",        value: "500" },  // 500ms 更新一次
    ]
}
```

### 6.2 关键规则

- `varFreqs` 中的 `name` 必须是 **完整变量名**（`pubNodeName_varName`）
- `value` 是字符串形式的毫秒数
- 一次请求可以包含多个变量，每个变量可以有不同的频率
- 对同一变量重复订阅会覆盖之前的频率

### 6.3 发布者处理

发布者收到 `SubTableRegister` 后：

1. **变量查找**：在 `varIndex_` 中查找每个变量的 hash
2. **懒创建**：如果变量在 `varIndex_` 中不存在但在 `liteBucketMembers_` 中存在，触发 `createVars` 创建
3. **频率更新**：在变量的 `freqSubs` 列表中添加该订阅者的频率条目
4. **频率重算**：`currentFreq = min(所有 freqSubs 的 freq)`
5. **调度更新**：标记 `schedulerDirty_`，后台线程重建定时器分组

### 6.4 频率协商示例

```
变量 "temperature" 的 freqSubs 变化：

初始状态:  freqSubs = []                         → currentFreq = 0xFFFFFFFF（无订阅者）

Sub1 订阅 100ms:
           freqSubs = [{freq=100, subMask=0x01}] → currentFreq = 100ms

Sub2 订阅 200ms:
           freqSubs = [{freq=100, subMask=0x01},
                       {freq=200, subMask=0x02}] → currentFreq = 100ms（取最小）

Sub3 订阅 50ms:
           freqSubs = [{freq=100, subMask=0x01},
                       {freq=200, subMask=0x02},
                       {freq=50,  subMask=0x04}] → currentFreq = 50ms

Sub3 退订:
           freqSubs = [{freq=100, subMask=0x01},
                       {freq=200, subMask=0x02}] → currentFreq = 100ms（回升）
```

---

## 7. 阶段三：数据传输（Pub → Sub）

### 7.1 传输调度

发布者按 `(bucketIndex, freqMs)` 分组，每组一个定时器：

```
分组示例：
  (bucket=0, freq=100ms) → 定时器每 100ms 触发
  (bucket=0, freq=200ms) → 定时器每 200ms 触发
  (bucket=1, freq=100ms) → 定时器每 100ms 触发
  ...
```

定时器首次触发有抖动（jitter）：`jitter = bucketIdx * max(freqMs/20, 5ms)`，避免所有 bucket 同时发送。

### 7.2 数据组包流程

定时器触发后，执行 `publishGroupData(bucketIndex, freqMs)`：

#### Step 1：批量读取变量数据

```
对分组内每个变量：
  1. 从 VarStore 零拷贝读取（seqlock 协议）
  2. 如果变量尚未写入过数据（valid_size == 0），跳过
  3. 将数据 memcpy 到 varData 列表
```

#### Step 2：构建 Roaring64Map 位图

```
情况 A：所有变量都有数据（无跳过）
  → 直接使用预计算的位图字节（零开销）

情况 B：部分变量被跳过
  → 构建新的 Roaring64Map，只包含实际发送的变量 hash
  → 序列化为字节流
```

#### Step 3：组装 TableDataTransfer

```
TableDataTransfer {
    mask:       <Roaring64Map 序列化字节>
    timestamp:  { tv_sec: 秒, tv_nsec: 纳秒 }
    varData:    [ blob0, blob1, blob2, ... ]  // 与 mask 迭代顺序一一对应
    blobType:   STRUCTS  (或其他序列化方式)
}
```

#### Step 4：发送

通过 DDS Writer 写入 Topic `dsf/var/data/transfer/bucket_N`。

### 7.3 mask 与 varData 的对应关系（核心！）

**这是对接方必须严格遵守的协议：**

```
mask (Roaring64Map) 序列化后的字节流
   ↓ 反序列化
Roaring64Map { hashA, hashC, hashF, hashG, ... }  // 升序排列
   ↓
varData[0] → 对应 hashA 的数据
varData[1] → 对应 hashC 的数据
varData[2] → 对应 hashF 的数据
varData[3] → 对应 hashG 的数据
...
```

**关键点：**
- Roaring64Map 的迭代顺序是 **升序**（从小到大）
- `varData` 数组的第 i 个元素对应位图迭代的第 i 个 hash
- 如果发送方跳过了某个变量（无数据），该 hash 不会出现在位图中，`varData` 也不会包含它

### 7.4 数据 blob 格式

每个 `varData` 元素是原始字节（`DT_BLOB` = `sequence<octet>`），内容取决于 `blobType`：

| blobType | 说明 | 序列化方式 |
|----------|------|-----------|
| `STRUCTS`（默认） | 结构体 | 原始内存拷贝（C struct 布局） |
| `YAS` | YAS 序列化 | 二进制序列化 |
| `NGVS` | NGVS 序列化 | 自定义格式 |
| `TX` | TX 格式 | 自定义格式 |

对接方通常只需处理 `STRUCTS` 类型，即 **原始字节拷贝**。

### 7.5 完整传输示例

假设 bucket 0 有 3 个变量被订阅：

```
变量信息：
  hash=0x1234, varId=0, data={0x01, 0x02, 0x03, 0x04} (4字节)
  hash=0x5678, varId=1, data={0x0A, 0x0B}             (2字节)
  hash=0xABCD, varId=2, (无数据，跳过)

Roaring64Map: {0x1234, 0x5678}  // 0xABCD 被跳过

TableDataTransfer {
    mask:       <序列化字节: [0x1234, 0x5678]>
    timestamp:  { tv_sec: 1700000000, tv_nsec: 123456789 }
    varData: [
        [0x01, 0x02, 0x03, 0x04],   // ← hash 0x1234 的数据
        [0x0A, 0x0B],                // ← hash 0x5678 的数据
    ]
    blobType: STRUCTS
}
```

---

## 8. 阶段四：取消订阅（Sub → Pub）

### 8.1 请求构造

```
SubTableRegister {
    msgType:    SUB_TABLE_UNREGISTER
    nodeName:   "subNode0"
    tableName:  "bucket_3"
    varFreqs: [
        { name: "pubNode0_temperature", value: "" },  // value 留空
        { name: "pubNode0_pressure",    value: "" },
    ]
}
```

### 8.2 发布者处理

1. 从变量的 `freqSubs` 中移除该订阅者的 bit
2. 如果某频率条目的 `subCount` 降为 0，删除该条目
3. 重新计算 `currentFreq`
4. 清理重试队列中该节点的所有待重试消息

---

## 9. 数据存储与读取

### 9.1 VarStore 内存布局

每个变量在内存中占一个 **Slot**，采用 **Seqlock 双缓冲** 机制：

```
Slot 内存布局（64 字节对齐）：
┌────────────────────────────────────────────────────────┐
│ 偏移   大小    字段           说明                       │
├────────────────────────────────────────────────────────┤
│ 0      4B     seq           原子序列号（奇数=写入中）      │
│ 4      1B     committed     当前可读缓冲区索引（0 或 1）  │
│ 5      4B     valid_size[0] 缓冲区 0 的有效数据长度       │
│ 9      4B     valid_size[1] 缓冲区 1 的有效数据长度       │
│ 13     51B    padding       对齐填充                      │
├────────────────────────────────────────────────────────┤
│ 64     N B    data[0]       缓冲区 0（N = payload 大小）  │
│ 64+N   N B    data[1]       缓冲区 1                     │
└────────────────────────────────────────────────────────┘

总大小 = align64(64 + 2 * payload_size)
其中 align64(n) = (n + 63) & ~63
```

### 9.2 Seqlock 写入协议

```
写入方（Publisher setVarData / Subscriber processDataTransfer）：

  1. seq += 1              // 变为奇数，表示"写入中"
  2. memory_fence(release)
  3. write_idx = 1 - committed   // 写入非活跃缓冲区
  4. memcpy(data[write_idx * size], src, actual_size)
  5. valid_size[write_idx] = actual_size
  6. committed = write_idx       // 切换活跃缓冲区
  7. seq += 1              // 变为偶数，表示"写入完成"
```

### 9.3 Seqlock 读取协议

```
读取方：

  loop:
    1. s1 = seq.load(acquire)
       如果 s1 是奇数 → 自旋等待（写入进行中）
    2. idx = committed.load(acquire)
    3. memcpy(dst, data[idx * size], valid_size[idx])
    4. s2 = seq.load(acquire)
       如果 s1 != s2 → 写入发生在读取期间，回到步骤 1 重试
```

### 9.4 零拷贝读取

读取方可获取指向 Slot 缓冲区的直接指针（无需 memcpy），但需要持有 `op_enter()` 引用计数防止内存重分配。读取完成后必须调用 `op_exit()` 释放。

---

## 10. 频率协商机制

### 10.1 变量元数据结构

```cpp
struct VarMetadata {
    uint32_t bucketIndex;           // 0~19
    uint32_t varId;                 // VarStore 中的槽位 ID
    uint32_t currentFreq;           // 当前发布频率（ms），0xFFFFFFFF = 无订阅者
    std::string realVarName;        // 变量名（不含节点前缀）

    struct FreqSub {
        uint32_t freq;              // 该订阅者请求的频率（ms）
        uint16_t subCount;          // 该频率的订阅者数量
        uint64_t subMask;           // 位掩码，每个 bit 对应一个订阅者节点
    };
    std::vector<FreqSub> freqSubs;  // 频率订阅列表
    uint8_t activeFreqCount;        // freqSubs.size()
};
```

### 10.2 频率计算规则

```
currentFreq = min(所有 freqSubs 中的 freq)
如果 freqSubs 为空 → currentFreq = 0xFFFFFFFF（不发布）
```

### 10.3 订阅者节点位掩码

系统支持最多 **64 个订阅者节点**。每个节点分配一个 bit 位（0~63），存储在 `subMask` 中。

```
subMask = 0x01 → 节点 0
subMask = 0x02 → 节点 1
subMask = 0x04 → 节点 2
...
subMask = 0x8000000000000000 → 节点 63
```

---

## 11. 哈希函数规范

系统使用 **FNV-1a 64 位** 哈希函数：

```cpp
uint64_t fast_hash(const std::string &str) {
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 0x100000001b3ULL;           // FNV prime
    }
    return hash;
}
```

### 变量名构造规则

```
完整变量名 = "{pubNodeName}_{varName}"

示例：
  pubNodeName = "pubNode0"
  varName = "temperature"
  完整变量名 = "pubNode0_temperature"
  hash = fast_hash("pubNode0_temperature")
  bucket = hash % 20
```

**对接方必须使用完全相同的哈希函数和变量名格式，否则 hash 不匹配导致无法通信。**

---

## 12. 断线重连机制

### 12.1 发布者断线检测

发布者通过 DDS 发现机制监控订阅者的 GUID。当订阅者断开时：

1. 检测到 GUID 消失
2. 自动清理该订阅者的所有订阅（调用 `handleUnsubscribe`）
3. 重新计算变量频率

### 12.2 订阅者断线恢复

订阅者检测到发布者断开时：

1. 缓存的 `PubTableDefine` 重新入队处理
2. 调用 `resendSubscriptions()` 重新发送所有订阅请求
3. 重新激活 DDS 分区

### 12.3 持久化 QoS

订阅注册使用 `TRANSIENT_LOCAL` + `RELIABLE` QoS，发布者重启后可从 DDS 中恢复之前的订阅请求（历史深度 32）。

---

## 13. 对接方实现 Checklist

### 13.1 必须实现的功能

- [ ] **DDS Domain 66** 上的通信
- [ ] **FNV-1a 哈希**：与 `fast_hash` 完全一致
- [ ] **变量名格式**：`"{pubNodeName}_{varName}"`
- [ ] **Bucket 分配**：`hash % 20`
- [ ] **订阅请求**：发送 `SubTableRegister` 到 `dsf/message/commandRequest/subTableRegister`
- [ ] **数据接收**：监听 `dsf/var/data/transfer/bucket_N`（N=0~19）
- [ ] **Roaring64Map 解析**：反序列化 mask，迭代顺序与 varData 一一对应
- [ ] **VarStore 写入**：Seqlock 双缓冲写入协议（如需存储数据）
- [ ] **频率协商**：理解 `currentFreq` 是所有订阅者的最小值

### 13.2 订阅流程（Sub 方实现）

```
1. 创建 DDS Domain 66 的 DomainParticipant
2. 创建 Subscriber / Publisher
3. 创建 DataReader 监听 Topic "dsf/sys/var/tableDefine"
4. 收到 PubTableDefine 后：
   a. 解析每个变量的 name、size
   b. 计算 hash = fast_hash(pubNodeName + "_" + name)
   c. 记录变量信息（hash → name/size 映射）
5. 创建 DataWriter 写入 Topic "dsf/message/commandRequest/subTableRegister"
6. 构造 SubTableRegister 消息：
   - msgType = SUB_TABLE_REGISTER
   - nodeName = 自己的节点名
   - varFreqs = [{name: 完整变量名, value: "频率ms"}, ...]
7. 发送订阅请求
8. 创建 DataReader 监听 Topic "dsf/var/data/transfer/bucket_N"
9. 收到 TableDataTransfer 后：
   a. 反序列化 mask（Roaring64Map::read）
   b. 遍历位图，第 i 个 hash 对应 varData[i]
   c. 根据 hash 查找变量信息
   d. 解析数据（STRUCTS 类型 = 原始字节拷贝）
```

### 13.3 数据接收伪代码

```python
# 收到 TableDataTransfer 消息后
def on_receive(msg):
    # 1. 反序列化 Roaring64Map
    mask = Roaring64Map.read(msg.mask)
    
    # 2. 遍历位图，与 varData 一一对应
    for i, var_hash in enumerate(mask):
        blob = msg.varData[i]
        
        # 3. 查找变量信息
        var_info = local_var_index.get(var_hash)
        if var_info is None:
            continue  # 未知变量，跳过
        
        # 4. 解析数据
        if msg.blobType == STRUCTS:
            # 原始字节拷贝，直接使用
            value = struct.unpack(var_info.format, bytes(blob))
        
        # 5. 触发用户回调
        user_callback(var_info.name, value, msg.timestamp)
```

### 13.4 常见陷阱

| 陷阱 | 说明 |
|------|------|
| 哈希不一致 | 必须使用 FNV-1a，seed=0xcbf29ce484222325，prime=0x100000001b3 |
| 变量名格式 | 必须是 `pubNodeName_varName`，不是单独的 `varName` |
| varData 顺序 | 必须与 Roaring64Map 升序迭代顺序一致，不能任意排列 |
| 频率值类型 | `varFreqs.value` 是字符串 `"100"`，不是整数 100 |
| Bucket Topic | 数据 Topic 是 `dsf/var/data/transfer/bucket_0` ~ `bucket_19`，不是单一 Topic |
| Seqlock | 写入时 seq 必须先加 1（奇数），写完再加 1（偶数），否则读者会读到不一致数据 |
| 位图序列化 | 使用 Roaring64Map 的 `write`/`read` 序列化，不是简单的大端字节序 |

---

## 附录 A：完整消息流示例

```
时间线：

T0: Pub 调用 registerVars(["var0", "var1", "var2"])
    → 广播 PubTableDefine:
       bucket_0: [{id:0, name:"var0"}, {id:1, name:"var1"}]
       bucket_5: [{id:0, name:"var2"}]

T1: Sub 收到 PubTableDefine
    → 本地注册: var0→id:0, var1→id:1, var2→id:2

T2: Sub 调用 subscribe("pubNode", [
       {varName:"var0", freq:100},
       {varName:"var1", freq:200},
       {varName:"var2", freq:100}
    ])
    → 发送 SubTableRegister:
       varFreqs: [
           {name:"pubNode_var0", value:"100"},
           {name:"pubNode_var1", value:"200"},
           {name:"pubNode_var2", value:"100"}
       ]

T3: Pub 收到订阅请求
    → var0: currentFreq = 100ms
    → var1: currentFreq = 200ms
    → var2: currentFreq = 100ms
    → 创建定时器: (bucket=0, freq=100ms), (bucket=0, freq=200ms), (bucket=5, freq=100ms)

T4: Pub 调用 setVarData("var0", data0, 4)
    → VarStore 写入 varId=0

T5: 定时器 (bucket=0, freq=100ms) 触发
    → 读取 var0 有数据, var1 无数据
    → 发送 TableDataTransfer on bucket_0:
       mask: [var0_hash]
       varData: [data0]

T6: 定时器 (bucket=0, freq=200ms) 触发
    → 读取 var0 有数据, var1 有数据
    → 发送 TableDataTransfer on bucket_0:
       mask: [var0_hash, var1_hash]
       varData: [data0, data1]

T7: Sub 收到 TableDataTransfer
    → 解析 mask, 找到 var0_hash → 对应 varData[0]
    → 写入本地 VarStore
    → 100ms 后触发用户回调
```

---

## 附录 B：Roaring64Map 序列化格式

Roaring64Map 使用 [Roaring Bitmap](https://roaringbitmap.org/) 库的标准序列化格式：

```
序列化（写入方）：
  Roaring64Map map;
  map.add(hash1);
  map.add(hash2);
  map.runOptimize();      // 压缩
  map.shrinkToFit();      // 释放多余内存
  size_t bytes = map.getSizeInBytes();
  std::vector<uint8_t> buf(bytes);
  map.write(reinterpret_cast<char*>(buf.data()));

反序列化（读取方）：
  Roaring64Map map = Roaring64Map::read(
      reinterpret_cast<const char*>(msg.mask().data()));
  
  // 迭代（升序）
  for (auto it = map.begin(); it != map.end(); ++it) {
      uint64_t varHash = *it;
      // varData[distance(map.begin(), it)] 对应该 hash 的数据
  }
```

对接方可使用任何语言的 Roaring Bitmap 实现：
- C/C++: [CRoaring](https://github.com/RoaringBitmap/CRoaring)
- Java: [RoaringBitmap](https://github.com/RoaringBitmap/RoaringBitmap)
- Python: [pyroaring](https://github.com/Ezibenroc/PyRoaringBitMap)
- Go: [roaring](https://github.com/RoaringBitmap/roaring)
- Rust: [roaring-rs](https://github.com/RoaringBitmap/roaring-rs)

---

## 附录 C：QoS 参数对照表

| Topic | 可靠性 | 持久性 | 历史 | 说明 |
|-------|--------|--------|------|------|
| `dsf/sys/var/tableDefine` | RELIABLE | TRANSIENT_LOCAL | depth=32 | 变量定义必须可靠送达 |
| `dsf/message/commandRequest/subTableRegister` | RELIABLE | TRANSIENT_LOCAL | depth=32 | 订阅请求必须可靠送达 |
| `dsf/var/data/transfer/bucket_N` | BEST_EFFORT | - | - | 数据传输允许丢失，按频率重发 |
