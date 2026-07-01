# OnDemand 轻量广播方案：Lite Topic + 按需创建

## 1. 与懒创建方案的区别

| | 懒创建方案 (registerVars) | 本方案 (Lite Topic) |
|---|---|---|
| 广播内容 | 完整 PubTableDefine（含 Define，~115MB） | 轻量 PubTableDefineLite（只有名字+大小，~4MB） |
| sub 存储 | 600K 个完整 Define (~285MB) | 600K 个 LiteVarInfo (~5MB) |
| pub 存储 | 600K 个完整 Define (216MB) | 600K 个 LiteVarInfo (~5MB) |
| subscribe 触发 | pub 懒创建 VarStore/varIndex_ | pub 真正 createVars + 广播完整 PubTableDefine |
| 总内存 (pub+sub, 100 订阅) | 217MB (pub) + 285MB (sub) = **502MB** | ~10MB (pub) + ~10MB (sub) = **20MB** |

**核心区别：** 本方案 pub 端连 Define 都不存，只存变量名和大小。
sub 端也不存完整 Define，只存变量名索引。

---

## 2. 流程

```
┌─── Phase 1: 注册（轻量）───────────────────────────────────────────┐
│                                                                     │
│  pub.registerVars(600K Define)                                      │
│    ├── 只提取 name + size，存入 liteVarIndex_ (5MB)                 │
│    ├── 广播 PubTableDefineLite（新 topic，轻量 IDL）                │
│    └── Define 本身不保存，用完即弃                                   │
│                                                                     │
│  sub 收到 PubTableDefineLite                                        │
│    └── 存入 subLiteVarIndex_ (5MB)：只记录变量名+大小+所属 pub      │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

┌─── Phase 2: 订阅（按需创建）───────────────────────────────────────┐
│                                                                     │
│  sub.subscribe(pubNode, ["v1","v2",...,"v100"], 100ms)              │
│    └── 发送 SubTableRegister（现有流程不变）                        │
│                                                                     │
│  pub 收到 SubTableRegister                                          │
│    ├── 检查 varIndex_：变量未创建                                   │
│    ├── 从 liteVarIndex_ 取 name/size                               │
│    ├── 真正 createVars()：分配 VarStore + varIndex_ + BucketManager │
│    └── 广播完整 PubTableDefine（现有 topic，sub 收到完整 Define）   │
│                                                                     │
│  sub 收到 PubTableDefine                                            │
│    └── 正常处理（现有流程不变）                                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. 新增 IDL

### 3.1 PubTableDefineLite（pub → sub，轻量广播）

```idl
// 变量名摘要（不含完整 Define）
struct PubTableVarSummary {
    uint16 id;          // bucket 内序号
    string name;        // 变量短名
    string nodeName;    // 所属 pub 节点名
    int32 size;         // 数据大小（sub 预分配用）
};

// 轻量变量表定义（替代全量 PubTableDefine 的广播）
@topic
struct PubTableDefineLite {
    @key string name;       // bucket 名称（如 "bucket_3"）
    @key string nodeName;   // pub 节点名
    sequence<PubTableVarSummary> varSummaries;
};
```

**单个变量大小：** ~60 bytes（vs Define 的 360 bytes），省 83%。
**600K 变量，20 bucket：** 每 bucket 30K × 60B = 1.8MB，总计 ~36MB DDS 传输。
但每次广播只发一个 bucket 的消息，单条消息 ~1.8MB，可接受。

### 3.2 SubTableRegister（不变）

现有 `SubTableRegister` 不需要改，sub 端仍然发送变量名+频率。

---

## 4. Pub 端设计

### 4.1 新增数据结构

```cpp
class OnDemandPub {
    // 轻量变量索引：只存 name + size，不含完整 Define
    struct LiteVarInfo {
        std::string name;       // 变量短名
        std::string nodeName;   // pub 节点名
        int32_t size;           // 数据大小
    };
    std::unordered_map<uint64_t, LiteVarInfo> liteVarIndex_;
    mutable std::shared_mutex liteVarIndexMutex_;

    // DDS Writer for PubTableDefineLite
    std::shared_ptr<DdsWrapper::DDSTopicWriter<PubTableDefineLite>> pubTableDefineLiteWriter_;
};
```

### 4.2 registerVars() 实现

```cpp
bool OnDemandPub::registerVars(const std::vector<DSF::Var::Define>& vars) {
    // 1. 提取轻量信息存入 liteVarIndex_
    std::unordered_set<uint32_t> affectedBuckets;
    {
        std::unique_lock lock(liteVarIndexMutex_);
        for (const auto& def : vars) {
            std::string metaName = make_meta_varname(nodeName_, def.name());
            uint64_t hash = fast_hash(metaName);
            uint32_t bucketIdx = BucketManager::CalculateBucketIndexFromHash(hash);

            if (liteVarIndex_.count(hash))
                continue;

            liteVarIndex_[hash] = LiteVarInfo{def.name(), nodeName_, def.size()};
            affectedBuckets.insert(bucketIdx);
        }
    }
    // Define 不保存，用完即弃

    // 2. 广播 PubTableDefineLite
    broadcastLiteDefines(affectedBuckets);
    return true;
}
```

### 4.3 broadcastLiteDefines() 实现

```cpp
void OnDemandPub::broadcastLiteDefines(const std::unordered_set<uint32_t>& bucketIds) {
    std::shared_lock lock(liteVarIndexMutex_);
    for (uint32_t bucketId : bucketIds) {
        PubTableDefineLite liteMsg;
        liteMsg.name(make_bucket_name_by_id(bucketId));
        liteMsg.nodeName(nodeName_);

        std::vector<PubTableVarSummary> summaries;
        for (const auto& [hash, info] : liteVarIndex_) {
            if (BucketManager::CalculateBucketIndexFromHash(hash) != bucketId)
                continue;
            PubTableVarSummary s;
            s.id(0);  // 序号，可选
            s.name(info.name);
            s.nodeName(info.nodeName);
            s.size(info.size);
            summaries.push_back(std::move(s));
        }

        if (!summaries.empty()) {
            liteMsg.varSummaries(std::move(summaries));
            pubTableDefineLiteWriter_->write(liteMsg);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}
```

### 4.4 handleSubscribe() 改造：按需 createVars

```cpp
void OnDemandPub::handleSubscribe(const SubTableRegister& request) {
    // ... 现有 nodeSlot 分配逻辑不变 ...

    // 收集需要创建的变量
    std::vector<DSF::Var::Define> pendingCreates;

    for (const auto& varFreq : request.varFreqs()) {
        uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varFreq.name()));

        // 检查是否已创建
        auto it = varIndex_.find(varHash);
        if (it == varIndex_.end()) {
            // 未创建 → 从 liteVarIndex_ 构造 Define
            std::shared_lock lock(liteVarIndexMutex_);
            auto liteIt = liteVarIndex_.find(varHash);
            if (liteIt == liteVarIndex_.end()) {
                ++missingCount;
                continue;
            }
            // 构造 Define 用于 createVars
            DSF::Var::Define def;
            def.name(liteIt->second.name);
            def.size(liteIt->second.size);
            def.nodeName(liteIt->second.nodeName);
            pendingCreates.push_back(std::move(def));
            continue;  // 创建后下一轮重试时再处理订阅
        }

        // 正常处理订阅...
    }

    // 批量创建缺失的变量
    if (!pendingCreates.empty()) {
        createVars(pendingCreates);  // 复用现有 createVars 逻辑
        // 触发重试：让这些变量在下一轮被正常订阅处理
        // （现有的 processReceiveRegister 重试机制会自动处理）
    }
}
```

**关键点：** `createVars()` 内部会广播完整的 `PubTableDefine`，
sub 端会收到完整 Define 并注册，然后 sub 的重试机制会重新发送 `SubTableRegister`，
这次 pub 端的 `varIndex_` 中已有变量，正常处理订阅。

---

## 5. Sub 端设计

### 5.1 新增数据结构

```cpp
class OnDemandSub {
    // 轻量变量索引（来自 PubTableDefineLite）
    struct SubLiteVarInfo {
        std::string name;
        std::string nodeName;   // 所属 pub 节点名
        int32_t size;
    };
    std::unordered_map<uint64_t, SubLiteVarInfo> subLiteVarIndex_;
    mutable std::shared_mutex subLiteVarIndexMutex_;

    // DDS Reader for PubTableDefineLite
    std::shared_ptr<DdsWrapper::DDSTopicReader<PubTableDefineLite>> pubTableDefineLiteReader_;
};
```

### 5.2 processTableDefineLite() 新增

```cpp
void OnDemandSub::processTableDefineLite() {
    PubTableDefineLite liteMsg;
    while (liteRunning_.load()) {
        if (pubTableDefineLiteReader_->takeNext(liteMsg, 100ms)) {
            std::unique_lock lock(subLiteVarIndexMutex_);
            for (const auto& summary : liteMsg.varSummaries()) {
                std::string metaName = make_meta_varname(summary.nodeName(), summary.name());
                uint64_t hash = fast_hash(metaName);
                subLiteVarIndex_[hash] = SubLiteVarInfo{
                    summary.name(), summary.nodeName(), summary.size()
                };
            }
            // 通知 TableDefineCallback（如果用户注册了）
            // 用户可以通过 subLiteVarIndex_ 查询可用变量
        }
    }
}
```

### 5.3 subscribe() 不变

sub 端 `subscribe()` 仍然发送 `SubTableRegister`，不需要改动。
sub 端不需要存完整 Define，只需要知道变量名（用于 subscribe）。

### 5.4 getAvailableVars() 适配

```cpp
// 现有：从 varDefineIndex_ 构造（包含完整 Define）
// 新增：从 subLiteVarIndex_ 构造（只有名字，无完整 Define）
// 两者合并返回
```

---

## 6. 内存对比

### Pub 端

| 数据结构 | createVars (当前) | Lite Topic (本方案) |
|----------|------------------|-------------------|
| Define 存储 | 216MB (defineCache_) | **0** (用完即弃) |
| LiteVarInfo | 0 | **5MB** (name+size) |
| VarStore | 37MB (600K Slot) | **0** (按需创建) |
| varIndex_ | 77MB (600K VarMetadata) | **0** (按需创建) |
| BucketManager | 29MB | **0** (按需创建) |
| **总计** | **~520MB** | **~5MB** |

### Sub 端

| 数据结构 | 当前 | Lite Topic (本方案) |
|----------|------|-------------------|
| varDefineIndex_ (完整 Define) | 285MB | **0** |
| subLiteVarIndex_ (轻量) | 0 | **5MB** |
| **总计** | **~285MB** | **~5MB** |

### 合计 (pub + sub, 100 变量被订阅)

| | 当前 | 本方案 |
|---|---|---|
| pub | 520MB | 5MB + 0.6MB(100 个 createVars) ≈ **6MB** |
| sub | 285MB | 5MB + 30KB(100 个 Define) ≈ **5MB** |
| **总计** | **805MB** | **~11MB** |

---

## 7. 时序图

```
  Pub                                    Sub
   │                                      │
   │  registerVars(600K)                  │
   │  ├─ liteVarIndex_ += 600K            │
   │  └─ broadcast PubTableDefineLite ──→ │
   │     (20 buckets, 每个 ~1.8MB)        │  subLiteVarIndex_ += 600K
   │                                      │
   │                                      │  user: subscribe(pubNode, ["v1"..v100"])
   │                                      │  sub sends SubTableRegister ──→
   │  ←────────────────────────────────── │
   │                                      │
   │  handleSubscribe()                   │
   │  ├─ v1~v100 不在 varIndex_           │
   │  ├─ 从 liteVarIndex_ 构造 Define     │
   │  ├─ createVars(100)                  │
   │  │  ├─ VarStore 分配 100 Slot        │
   │  │  ├─ varIndex_ += 100              │
   │  │  ├─ BucketManager += 100          │
   │  │  └─ 广播 PubTableDefine (完整) ──→│
   │  │                                   │  processTableDefine()
   │  │                                   │  varDefineIndex_ += 100
   │  └─ 处理订阅（freqSubs 等）          │
   │                                      │
   │  setVarData("v1", data)              │
   │  ├─ varIndex_ 找到 → 正常写入        │
   │                                      │  收到数据回调 ✓
   │                                      │
   │  setVarData("v50000", data)          │
   │  ├─ varIndex_ 找不到 → NOT_READY     │  (没人订阅 v50000，不需要写)
   │                                      │
```

---

## 8. 向后兼容

| API | 行为变化 |
|-----|---------|
| `registerVars()` | **新增**：轻量注册，广播 Lite，不分配 VarStore |
| `createVars()` | 不变：立即创建，完整分配 |
| `subscribe()` | 不变：sub 端无感知 |
| `setVarData()` | 不变：未创建返回 NOT_READY |
| `deleteVars()` | 需适配 liteVarIndex_ 清理 |

**DDS 消息：**
- 新增 `PubTableDefineLite` topic（`dsf/sys/var/tableDefineLite`）
- 现有 `PubTableDefine`、`SubTableRegister`、`TableDataTransfer` 不变
- sub 端新增 `PubTableDefineLite` reader，不影响现有 reader

---

## 9. 风险

### 9.1 subscribe 后 createVars 的时序

pub 收到 subscribe → createVars(100) → 广播 PubTableDefine → sub 收到 → sub 重新订阅。
这个过程需要 1~2 个 RTT，sub 端有现有重试机制（200ms~3s 指数退避）处理。

### 9.2 Lite 和 Full 广播共存

sub 端同时接收 `PubTableDefineLite` 和 `PubTableDefine` 两个 topic。
Lite 用于查询可用变量，Full 用于获取完整 Define（subscribe 触发后才发）。
需要区分处理，避免重复注册。

### 9.3 liteVarIndex_ 的 deleteVars 适配

删除变量时需清理 liteVarIndex_。
广播 Lite 消息时需反映删除后的状态（差量删除）。

---

## 10. 实现步骤

### Phase 1: IDL + DDS 通道（1天）
- 定义 `PubTableDefineLite`、`PubTableVarSummary` IDL
- 生成 DDS 类型代码
- pub 创建 Lite Writer，sub 创建 Lite Reader

### Phase 2: Pub 端 registerVars + broadcastLite（1天）
- 实现 `liteVarIndex_` 数据结构
- 实现 `registerVars()`
- 实现 `broadcastLiteDefines()`

### Phase 3: Pub 端 handleSubscribe 按需创建（1天）
- 改造 `handleSubscribe()`：从 liteVarIndex_ 构造 Define → createVars
- 复用现有 createVars 广播完整 PubTableDefine

### Phase 4: Sub 端 Lite 接收（半天）
- 实现 `subLiteVarIndex_` 数据结构
- 实现 `processTableDefineLite()`
- 适配 `getAvailableVars()`

### Phase 5: 测试（1天）
- registerVars + subscribe 触发创建的端到端测试
- 现有 27 个用例回归
- 内存测试：600K registerVars + 100 subscribe
