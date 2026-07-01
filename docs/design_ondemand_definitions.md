# OnDemand 大规模变量内存优化方案

## 1. 问题

### Pub 端内存浪费

600K 变量调用 `createVars()` 后，pub 端立即分配 ~520MB 持久内存，即使零订阅者：

```
createVars(600K)
  ├── defineCache_:   600K × 360B  = 216MB  ← Define 拷贝，广播用
  ├── VarStore:       600K × 64B   =  37MB  ← Slot header，没人写也要分配
  ├── varIndex_:      600K × 128B  =  77MB  ← VarMetadata，热路径索引
  ├── BucketManager:  600K × 48B   =  29MB  ← hash 注册
  ├── VarStore::table_: 600K × 56B =  34MB  ← hash→id 映射
  └── 其他:                             27MB
  总计:                               ~520MB
```

**核心矛盾：** 用户注册 600K 变量，实际只发布/订阅其中 100 个。
剩下 599900 个变量的 VarStore slot、varIndex_ 条目、BucketManager 注册全部白分配。

---

## 2. 方案：懒创建（Lazy Create）

### 核心思路

新增 `registerVars()` API，只存 Define 不分配重结构。变量在首次被 subscribe 触发时才真正创建。

```
registerVars(600K)   →  只存 Define 到 defineCache_（216MB）
                       不分配 VarStore / varIndex_ / BucketManager
                       广播 PubTableDefine 通知 sub 有哪些变量可用

sub.subscribe("v1"~"v100")
  → pub handleSubscribe() 收到请求
  → 检查 varIndex_：变量未创建
  → 检查 defineCache_：变量已注册（registerVars 存过）
  → lazyCreateVar()：从 defineCache_ 取 Define，分配 VarStore + varIndex_ + BucketManager
  → 正常处理订阅
```

### 内存对比（pub 端）

| 场景 | 当前 (createVars) | 优化后 (registerVars) | 节省 |
|------|-------------------|----------------------|------|
| 600K 注册，0 订阅 | 520MB | 216MB | **304MB (58%)** |
| 600K 注册，100 订阅 | 520MB | 217MB | **303MB (58%)** |
| 600K 注册，10K 订阅 | 520MB | 223MB | **297MB (57%)** |
| 600K 注册，全部订阅 | 520MB | 520MB | 0 |

---

## 3. 详细设计

### 3.1 新增 API

```cpp
class OnDemandPub {
public:
    /**
     * @brief 注册变量定义（轻量，不分配 VarStore/varIndex_）
     *
     * 只将 Define 存入 defineCache_，广播 PubTableDefine 通知 sub。
     * 不分配 VarStore slot、不创建 varIndex_ 条目、不注册 BucketManager。
     * 变量在首次被 subscribe 触发时才真正创建（懒创建）。
     *
     * 适用场景：变量数量远大于实际发布/订阅量（如注册 600K，实际只用 100 个）。
     *
     * @param vars 变量定义列表
     * @return true 成功
     */
    bool registerVars(const std::vector<DSF::Var::Define>& vars);

    // createVars() 保持不变（立即创建，完整分配）
};
```

### 3.2 新增数据结构

```cpp
class OnDemandPub {
    // 已注册（registerVars）但未创建（createVars/lazyCreate）的变量 hash 集合
    // defineCache_ 中有 Define，但 varIndex_/VarStore/BucketManager 中无条目
    std::unordered_set<uint64_t> pendingVars_;
    mutable std::mutex pendingVarsMutex_;
};
```

### 3.3 registerVars() 实现

```cpp
bool OnDemandPub::registerVars(const std::vector<DSF::Var::Define>& vars) {
    std::unordered_set<uint32_t> affectedBuckets;

    {
        std::unique_lock lock(varIndexMutex_);
        defineLookup_.reserve(defineLookup_.size() + vars.size());

        for (const auto& def : vars) {
            std::string metaName = make_meta_varname(nodeName_, def.name());
            uint64_t hash = fast_hash(metaName);
            uint32_t bucketIdx = BucketManager::CalculateBucketIndexFromHash(hash);

            // 去重：已在 defineCache_ 中的跳过
            if (defineLookup_.count(hash))
                continue;

            // 只存 Define，不分配 VarStore/varIndex_/BucketManager
            defineLookup_[hash] = static_cast<uint32_t>(defineCache_.size());
            defineCache_.push_back(def);
            affectedBuckets.insert(bucketIdx);
        }
    }

    // 标记 pending（独立锁，不与 varIndexMutex_ 嵌套）
    {
        std::lock_guard<std::mutex> lock(pendingVarsMutex_);
        for (const auto& def : vars) {
            uint64_t hash = fast_hash(make_meta_varname(nodeName_, def.name()));
            pendingVars_.insert(hash);
        }
    }

    // 广播 PubTableDefine（sub 需要知道有哪些变量可用）
    broadcastTableDefines(affectedBuckets);
    return true;
}
```

### 3.4 handleSubscribe() 改造

在现有逻辑的变量查找处增加懒创建分支：

```cpp
// on_demand_pub.cc handleSubscribe() 中，对每个请求的变量：

auto it = varIndex_.find(varHash);
if (it == varIndex_.end()) {
    // 变量未创建 → 尝试懒创建
    if (!tryLazyCreateVar(varHash)) {
        // 既不在 varIndex_ 也不在 defineCache_，真的不存在
        ++missingCount;
        continue;
    }
    it = varIndex_.find(varHash);
}

// 后续正常处理订阅（ensure_capacity、freqSubs 更新等）...
```

### 3.5 tryLazyCreateVar() 实现

```cpp
bool OnDemandPub::tryLazyCreateVar(uint64_t varHash) {
    // 1. 检查是否在 pending 中
    {
        std::lock_guard<std::mutex> lock(pendingVarsMutex_);
        if (!pendingVars_.count(varHash))
            return false;
    }

    // 2. 从 defineCache_ 取 Define（需要 varIndexMutex_ 读锁）
    const DSF::Var::Define* define = nullptr;
    uint32_t bucketIdx = 0;
    {
        std::shared_lock lock(varIndexMutex_);
        auto idxIt = defineLookup_.find(varHash);
        if (idxIt == defineLookup_.end())
            return false;
        define = &defineCache_[idxIt->second];
        bucketIdx = static_cast<uint32_t>(
            BucketManager::CalculateBucketIndexFromHash(varHash));
    }

    // 3. 分配 VarStore slot（ConfigGuard，会短暂阻塞读写）
    uint32_t sizes[] = {0u};
    uint32_t ids[] = {0u};
    uint64_t hashes[] = {varHash};
    varStore_.register_var_batch(hashes, sizes, ids, 1);

    // 4. 创建 varIndex_ 条目 + 注册 BucketManager（写锁）
    {
        std::unique_lock lock(varIndexMutex_);
        VarMetadata meta;
        meta.currentFreq = 0xFFFFFFFF;
        meta.activeFreqCount = 0;
        meta.bucketIndex = bucketIdx;
        meta.realVarName = define->name();
        meta.varId = ids[0];
        varIndex_[varHash] = std::move(meta);
        bucketManager_.AddMember(varHash);
    }

    // 5. 从 pending 中移除
    {
        std::lock_guard<std::mutex> lock(pendingVarsMutex_);
        pendingVars_.erase(varHash);
    }

    return true;
}
```

### 3.6 信息完整性保证

subscribe 触发时 pub 端所有必要信息的来源：

| 信息 | 来源 | 何时可用 |
|------|------|---------|
| 变量名 (realVarName) | `defineCache_[idx].name()` | registerVars 时 |
| 数据大小 (size) | `defineCache_[idx].size()` | registerVars 时 |
| VarStore slot | `varStore_.register_var_batch()` | lazyCreate 时 |
| varIndex_ 条目 (varId, bucketIndex) | lazyCreateVar 构造 | lazyCreate 时 |
| BucketManager 注册 | `bucketManager_.AddMember()` | lazyCreate 时 |
| DDS DataTransfer writer | `createDataTransferWriter()` | registerVars 时（按 bucket 创建） |

**所有信息在 subscribe 处理时都已就绪**，不存在缺失。lazyCreateVar 在 handleSubscribe 的变量循环内同步完成，后续的 ensure_capacity / freqSubs 更新正常执行。

### 3.7 setVarData() 不需要改

变量未创建时（不在 varIndex_ 中），write() 返回 `WriteResult::NOT_READY`。
这是正确的语义：没人订阅，不需要写数据。

### 3.8 广播路径适配

registerVars 注册的变量不在 BucketManager 中，广播时需要从 defineLookup_ 遍历：

```cpp
void OnDemandPub::broadcastTableDefines(const std::unordered_set<uint32_t>& bucketIds) {
    std::shared_lock lock(varIndexMutex_);
    for (uint32_t bucketId : bucketIds) {
        DSF::Var::PubTableDefine msg;
        msg.name(make_bucket_name_by_id(bucketId));
        msg.nodeName(nodeName_);
        msg.description("onDemandPub TableDefine");

        for (const auto& [hash, idx] : defineLookup_) {
            if (BucketManager::CalculateBucketIndexFromHash(hash) != bucketId)
                continue;
            DSF::Var::PubTableVarDefine varDef;
            DSF::Var::VarRequest req;
            req.varDefine(defineCache_[idx]);
            varDef.var(std::move(req));
            msg.varDefines().push_back(std::move(varDef));
        }

        if (!msg.varDefines().empty()) {
            tableDefinePublish(msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}
```

### 3.9 deleteVars() 适配

删除变量时需同时处理 pending 状态：

```cpp
bool OnDemandPub::deleteVars(const std::vector<std::string>& varNames) {
    // ... 现有逻辑 ...
    for (const auto& varName : varNames) {
        uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varName));

        // 从 pending 中移除
        {
            std::lock_guard<std::mutex> lock(pendingVarsMutex_);
            pendingVars_.erase(varHash);
        }

        // 从 defineCache_ 中移除
        {
            std::unique_lock lock(varIndexMutex_);
            defineLookup_.erase(varHash);
            // 注意：defineCache_ 是 vector，不能直接 erase，需要标记或重建
        }

        // 如果已创建，正常清理 varIndex_/VarStore/BucketManager
        auto it = varIndex_.find(varHash);
        if (it != varIndex_.end()) {
            // ... 现有删除逻辑 ...
        }
    }
}
```

---

## 4. 向后兼容

| API | 行为变化 |
|-----|---------|
| `createVars()` | 不变（立即创建，完整分配） |
| `registerVars()` | **新增**（轻量注册，懒创建） |
| `subscribe()` | 不变（pub 侧自动懒创建，sub 无感知） |
| `setVarData()` | 不变（未创建返回 NOT_READY） |
| `deleteVars()` | 不变（需适配 pending 变量的删除） |
| `getAvailableVars()` | 不变（从 defineCache_ 构造，含 pending） |

DDS 消息格式不变，sub 端无需改动。

### 迁移路径

```cpp
// 旧代码：
pub.createVars(allDefines);  // 600K 变量，520MB

// 新代码：
pub.registerVars(allDefines);  // 600K 变量，216MB
// subscribe 时自动懒创建，无需改 sub 端代码
```

---

## 5. 风险

### 5.1 首次 subscribe 延迟

100 个变量的懒创建 ≈ 100 × (VarStore 注册 + varIndex_ 插入) ≈ 0.1ms，可忽略。

### 5.2 广播遍历 defineLookup_ 的开销

广播时遍历全量 defineLookup_ 按 bucket 筛选。600K 条目 ≈ 几 ms，可接受。
优化：维护 per-bucket 的 pending hash 集合，避免全量遍历。

### 5.3 锁顺序

`tryLazyCreateVar()` 中锁顺序：`pendingVarsMutex_` → `varIndexMutex_`。
`handleSubscribe()` 中先持 `varIndexMutex_` 读锁再调用 `tryLazyCreateVar()`，
需要先释放读锁再调用，避免与写锁死锁。

### 5.4 defineCache_ 的 deleteVars 适配

`defineCache_` 是连续 vector，不支持 O(1) erase。
方案：用 tombstone 标记已删除，或重建索引。

---

## 6. 实现步骤

### Phase 1: 新增 pendingVars_ + registerVars()（1天）
- 新增 `pendingVars_` 数据结构
- 实现 `registerVars()`

### Phase 2: handleSubscribe 懒创建（1天）
- 实现 `tryLazyCreateVar()`
- 改造 `handleSubscribe()` 增加懒创建分支

### Phase 3: 广播 + deleteVars 适配（半天）
- 实现 `broadcastTableDefines()`
- 适配 `deleteVars()` 的 pending 处理

### Phase 4: 测试（1天）
- registerVars + subscribe 触发创建的测试用例
- 现有 27 个用例回归
- 内存测试：600K registerVars + 100 subscribe
