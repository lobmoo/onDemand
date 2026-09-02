# Monitor修复总结 - NACK重传 + Subscriber字节统计

## 已修复的两个关键Bug

### Bug 1: NACK了但重传包计数为0
**根因：** OnData中reader统计逻辑错误，检查writer的participant是否为sub（永远为false）

**修复（metrics_engine.cc:651-696）：**
```cpp
// 旧逻辑：检查writer是否属于sub participant（永远false）
GUID_t writer_participant = GetParticipantGuid(data.writer_guid);
bool is_sub_participant = (discovered_pub_sub_.find(writer_participant)->second == false);

// ✅ 新逻辑：遍历所有subscriber，为每个创建/更新reader endpoint
for (const auto& [p_guid, is_pub] : discovered_pub_sub_) {
    if (!is_pub) {  // is subscriber
        GUID_t reader_guid;
        reader_guid.prefix = p_guid.prefix;  // 用subscriber的prefix
        reader_guid.entityId = {0, 0, writer_entity_idx, 0x04};
        
        // 创建reader endpoint并注册到topic_readers_索引
        auto& reader_ep = endpoints_[reader_guid];
        reader_ep.data_count++;
        reader_ep.bytes_sent += data.payload_size;
        TrackRetransmitAndGaps(reader_ep, data.seq_num, timestamp_us);  // ← 重传检测
    }
}
```

**效果：**
- 每个subscriber的reader endpoint现在都能正确统计DATA包
- TrackRetransmitAndGaps的901-920行NACK匹配逻辑能正确执行
- reader端的retransmit_count现在能正确增长

---

### Bug 2: 多sub单pub场景，sub端DataTransfer字节为0
**根因：** 同Bug 1，reader endpoint未被正确创建/统计

**修复1（OnData处理）：** 同上，遍历所有subscriber创建reader

**修复2（OnFragment处理，metrics_engine.cc:1036-1102）：**
```cpp
// 旧逻辑：仅处理frag.reader_guid（multicast时为ENTITYID_UNKNOWN）
if (has_valid_reader) {
    // 只有明确的reader_guid才统计
}

// ✅ 新逻辑：先处理明确reader，再遍历所有subscriber
// 1. 处理显式reader_guid（如果有）
if (has_valid_reader) { ... }

// 2. 遍历所有subscriber，更新fragment统计
for (const auto& [p_guid, is_pub] : discovered_pub_sub_) {
    if (!is_pub) {
        GUID_t reader_guid = ConstructReaderGuid(p_guid, frag.writer_guid);
        auto& reader_ep = endpoints_[reader_guid];
        reader_ep.frag_count += frag.frag_count;
        reader_ep.bytes_sent += frag.payload_size;
        reader_ep.data_count++;
        TrackRetransmitAndGaps(reader_ep, frag.seq_num, timestamp_us);
    }
}
```

**效果：**
- subscriber端的bucket_N reader现在能正确显示字节数
- 支持多subscriber场景，每个sub独立统计
- 覆盖DATA和DATA_FRAG两种submessage

---

## 验证方法

启动1pub + 2sub场景：
```bash
# Terminal 1: Publisher (1w points)
./build/demo_exec pub 10000

# Terminal 2: Subscriber 1 (1Hz)
./build/demo_exec sub 1000

# Terminal 3: Subscriber 2 (10Hz)
./build/demo_exec sub 100

# Terminal 4: Monitor
./build/tools/ondemand_monitor/ondemand_monitor
```

**期望UI输出：**
```
Participant: demo_pub (01.02.03.04)
  Endpoints: 20
    dsf/var/data/transfer/bucket_0 (W)
      DataMsg: 500  Byte: 1.2 MB  Retransmit: 5  Lost: 2

Participant: demo_sub_1 (05.06.07.08)
  Endpoints: 20
    dsf/var/data/transfer/bucket_0 (R)
      DataMsg: 500  Byte: 1.2 MB  Retransmit: 5  Lost: 0  ✅

Participant: demo_sub_2 (09.0A.0B.0C)
  Endpoints: 20
    dsf/var/data/transfer/bucket_0 (R)
      DataMsg: 500  Byte: 1.2 MB  Retransmit: 3  Lost: 0  ✅
```

---

## 技术细节

### Reader Endpoint创建的3个时机

1. **SPDP发现时（252-272行）** —— 最早，为新subscriber预创建20个bucket readers
   ```cpp
   if (is_new && is_sub) {
       for (uint32_t bucket = 0; bucket < 20; bucket++) {
           // 创建reader endpoint + 注册到topic_readers_
       }
   }
   ```

2. **SEDP announcements（392-518行）** —— subscriber主动announce的reader
   ```cpp
   if (IsBuiltinDiscoveryEntity(data.writer_guid)) {
       // 解析SEDP payload，创建真实reader endpoint
       topic_readers_[topic_name].push_back(reader_guid);
   }
   ```

3. **DATA/FRAG fallback（本次修复）** —— 兜底机制，晚启动/漏包时补救
   ```cpp
   // OnData/OnFragment中，遍历discovered_pub_sub_创建reader
   ```

### 为什么需要遍历discovered_pub_sub_

**Multicast场景特点：**
- Publisher发1个包 → 所有subscriber都收到
- DATA包的reader_guid = ENTITYID_UNKNOWN（DDS规范）
- Monitor无法从单个包判断"哪些subscriber收到了"

**解决方案：**
- 维护discovered_pub_sub_ (participant_guid → is_publisher)
- 每收到一个DATA包，假设**所有已知subscriber都收到了**
- 为每个subscriber的对应reader endpoint更新统计

**准确性：**
- 轻微过估计：如果某个sub实际丢包，monitor仍认为它收到了
- 但NACK会纠正：丢包的sub会发NACK，monitor记录到nacked_requests
- 重传包到达时，只有发过NACK的reader才增retransmit_count（901-920行）

---

## 编译与部署

```bash
cd /home/wwk/workspace/onDemand/build
make ondemand_monitor -j$(nproc)

# 部署到docker（如需要）
docker cp build/tools/ondemand_monitor/ondemand_monitor <container>:/workspace/onDemand/build/tools/ondemand_monitor/
```

---

## 相关文件

- `tools/ondemand_monitor/metrics_engine.cc` — 核心修复
- `tools/ondemand_monitor/BUGFIX_NACK_READER.md` — 详细分析文档
- `tools/ondemand_monitor/BUGFIX_PORT_FILTER.md` — 之前修复的BPF过滤器问题
