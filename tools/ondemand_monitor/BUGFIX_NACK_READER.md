# Bug分析：NACK重传统计 + Subscriber端字节统计

## 问题1：NACK了但重传包为0

### 现象
UI显示 `NACK: 50  Retransmit: 0` —— 明明发了NACK请求，但重传包计数为0

### 根因
**OnData中的reader统计逻辑缺失topic_readers_注册**

```cpp
// metrics_engine.cc:632-687 OnData处理
if (!endpoint.topic_name.empty()) {
    auto reader_it = topic_readers_.find(endpoint.topic_name);
    if (reader_it != topic_readers_.end()) {
        for (const auto& rg : reader_it->second) {
            // 只有在topic_readers_中注册的reader才会统计
            ep.data_count++;
            ep.bytes_sent += data.payload_size;
            TrackRetransmitAndGaps(ep, data.seq_num, timestamp_us);  // ← 重传检测
        }
    }
}
```

**问题链条：**
1. OnAcknack记录了 `writer_ep.nacked_requests[sn] = {reader_guid}`
2. 重传包到达时OnData调用 `TrackRetransmitAndGaps(reader_ep, sn, ...)`
3. **但reader_ep根本没进入统计分支**，因为它不在`topic_readers_[topic_name]`里
4. 所以901-920行的NACK匹配逻辑永远不会执行到reader端

### 多sub场景更糟
- sub1的reader不在topic_readers_里 → 它的重传统计为0
- sub2的reader也不在 → 它的重传统计也为0
- writer端的retransmit_count能统计到（915-916行），但**reader看不到自己收到的重传包**

---

## 问题2：多sub单pub场景，sub端DataTransfer字节为0

### 现象
```
Publisher:  bucket_0 writer: 1.2 MB ✅
Subscriber: bucket_0 reader: 0 B    ❌
```

### 根因
**相同：reader endpoint未注册到topic_readers_索引**

OnData的632-650行依赖topic_readers_查找reader：
```cpp
auto reader_it = topic_readers_.find(endpoint.topic_name);
if (reader_it != topic_readers_.end()) {
    for (const auto& rg : reader_it->second) {
        ep.bytes_sent += data.payload_size;  // ← sub端字节统计
    }
}
if (!found_reader) {
    // 654-686行：创建virtual reader的逻辑
    // 但这个逻辑有问题：只在writer的participant是sub时创建
    // 实际上应该是：为所有sub participant创建reader
}
```

**654-660行的判断错了：**
```cpp
GUID_t writer_participant = GetParticipantGuid(data.writer_guid);
auto pubsub_it = discovered_pub_sub_.find(writer_participant);
bool is_sub_participant = (pubsub_it != discovered_pub_sub_.end() && !pubsub_it->second);
```
这是在检查**writer所属的participant是否为sub** —— 但writer永远属于publisher！
所以这个分支永远不会创建virtual reader。

---

## 根本原因：SEDP发现不完整

**正常流程应该是：**
1. Subscriber启动 → 发SEDP announcements（订阅bucket_0的reader）
2. Monitor抓到SEDP → 在OnData的392-518行创建reader endpoint
3. 439行注册：`topic_readers_[sedp_endpoint.topic_name].push_back(real_endpoint_guid);`
4. 后续DATA包到达时，632-650行能找到reader并统计

**但实际：**
- Monitor可能晚于subscriber启动（错过初始SEDP）
- 或者SEDP的reader announcement没抓到（BPF过滤器/丢包）
- 导致`topic_readers_`为空，所有reader统计失效

**备用创建机制（252-272行）在SPDP时创建20个bucket readers：**
```cpp
if (is_new && is_sub) {
    for (uint32_t bucket = 0; bucket < 20; bucket++) {
        // 创建 reader_guid {prefix, 00 00 (bucket+3) 04}
        ep.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket);
        // ❌ 但没有调用 topic_readers_[topic_name].push_back(reader_guid)
    }
}
```
**这就是bug！** 创建了endpoint但没注册到topic_readers_索引！

---

## 修复方案

### Fix 1: SPDP创建reader时注册到索引（优先）

```cpp
// metrics_engine.cc:252-272
if (is_new && is_sub) {
    for (uint32_t bucket = 0; bucket < 20; bucket++) {
        GUID_t reader_guid;
        // ... 构造GUID ...
        
        UpdateEndpoint(reader_guid, false, timestamp_us);
        auto& ep = endpoints_[reader_guid];
        ep.is_reader = true;
        ep.topic_name = "dsf/var/data/transfer/bucket_" + std::to_string(bucket);
        ep.type_name = "TableDataTransfer";
        
        // ✅ FIX: 注册到topic_readers_索引
        topic_readers_[ep.topic_name].push_back(reader_guid);
    }
}
```

### Fix 2: OnData的virtual reader创建逻辑修正

```cpp
// metrics_engine.cc:654-686
if (!found_reader) {
    // ✅ 遍历所有subscriber participants，为每个创建virtual reader
    for (const auto& [p_guid, is_pub] : discovered_pub_sub_) {
        if (!is_pub) {  // is subscriber
            GUID_t virtual_reader_guid;
            virtual_reader_guid.prefix = p_guid.prefix;  // subscriber的prefix
            virtual_reader_guid.entityId[0] = 0x00;
            virtual_reader_guid.entityId[1] = 0x00;
            virtual_reader_guid.entityId[2] = data.writer_guid.entityId[2];  // 同bucket
            virtual_reader_guid.entityId[3] = 0x04;
            
            // 检查是否已存在
            if (endpoints_.find(virtual_reader_guid) == endpoints_.end()) {
                auto& vr = endpoints_[virtual_reader_guid];
                vr.guid = virtual_reader_guid;
                vr.is_reader = true;
                vr.topic_name = endpoint.topic_name;
                vr.type_name = endpoint.type_name;
                vr.participant_guid = p_guid;
                topic_readers_[endpoint.topic_name].push_back(virtual_reader_guid);
            }
            
            // 统计到这个sub的reader
            auto& vr = endpoints_[virtual_reader_guid];
            vr.data_count++;
            vr.bytes_sent += data.payload_size;
            vr.last_seen_us = timestamp_us;
            TrackRetransmitAndGaps(vr, data.seq_num, timestamp_us);
        }
    }
}
```

### 验证方法

启动 `1pub + 2sub`：
```bash
# Pub: 1w points
./demo_exec pub 10000

# Sub1: 1Hz
./demo_exec sub 1000

# Sub2: 10Hz
./demo_exec sub 100

# Monitor
./ondemand_monitor
```

**期望结果：**
```
Participant: demo_pub
  Topic: bucket_0 (W)  Byte: 1.2 MB  DataMsg: 500  Retransmit: 5

Participant: demo_sub_1
  Topic: bucket_0 (R)  Byte: 1.2 MB  DataMsg: 500  Retransmit: 5  ✅

Participant: demo_sub_2
  Topic: bucket_0 (R)  Byte: 1.2 MB  DataMsg: 500  Retransmit: 3  ✅
```
