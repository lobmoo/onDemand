/**
 * @file on_demand_pub.cc
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2026-03-05
 * 
 * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2026-03-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */

#include "on_demand_pub.h"
#include <algorithm>
#include <charconv>

#include "roaring/roaring64map.hh"

namespace dsf
{
namespace ondemand
{

    OnDemandPub::OnDemandPub()
        : varIndex_(), varIndexMutex_(), bucketManager_(), initialized_(false), running_(false),
          dataNode_(nullptr), nodeName_(), pubTableDefineWriter_(nullptr),
          subTableRegisterReqReader_(nullptr), freqChangeCb_(nullptr)
    {
    }

    void OnDemandPub::setFreqChangeCallback(FreqChangeCallback cb)
    {
        std::lock_guard<std::mutex> lock(freqChangeCbMutex_);
        freqChangeCb_ = std::move(cb);
    }

    OnDemandPub::~OnDemandPub() { stop(); }

    /**
     * @brief 初始化发布者节点
     * @param  nodeName 节点名称，需全局唯一
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::init(const std::string &nodeName)
    {
        if (initialized_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already initialized";
            return false;
        }

        nodeName_ = nodeName;

        /*创建节点*/
        dataNode_ = createDataNode(nodeName, this);
        if (!dataNode_) {
            ONDEMANDLOG(error) << "Failed to create DataNode";
            initialized_.store(false);
            return false;
        }

        /*创建数据通信所需puiblisher*/
        DdsWrapper::PublisherQoSBuilder pubQos;
        pubQos.setPartition(
            DSF::Var::VAR_DATA_TRANSFER_TOPIC_PREFIX
            + nodeName_); // 按 topicName + nodeName 分区，确保同一节点不同表间隔离，避免不相关的订阅者收到数据

        if (!dataNode_->createPublisher(DATA_TANSFER_PUB_SUB_NAME, pubQos)) {
            ONDEMANDLOG(error)
                << "Failed to create Publisher for DataTransfer writers with partition: ";
            return false;
        }

        /*创建变量通知topic writer*/
        if (!createTableDefineWriter()) {
            ONDEMANDLOG(error) << "Failed to create PubTableDefine writer";
            initialized_.store(false);
            return false;
        }

        /*创建接收频率请求topic reader*/
        if (!createSubTableRegisterReader(std::bind(&OnDemandPub::onReceiveRegisterCb, this,
                                                    std::placeholders::_1,
                                                    std::placeholders::_2))) {
            ONDEMANDLOG(error) << "Failed to create SubTableRegister reader";
            initialized_.store(false);
            return false;
        }

        /*确保 DDS endpoints 就绪: assertLiveliness 强制发送 PDP 心跳*/
        dataNode_->assertLiveliness();
        ONDEMANDLOG(info) << "OnDemandPub initialized: " << nodeName;

        return true;
    }

    /**
     * @brief 启动发布者节点，开始处理订阅请求和发布数据
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::start()
    {
        if (!initialized_.load(std::memory_order_acquire)) {
            ONDEMANDLOG(error) << "OnDemandPub not initialized";
            return false;
        }
        if (running_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already running";
            return false;
        }

        /*创建时间轮调度器: 1ms tick 精度，线程池 4 线程*/
        const size_t poolSize = 4;
        publishScheduler_ = std::make_unique<TimerScheduler>(1, poolSize);

        registerProcessThread_ = std::thread(&OnDemandPub::processReceiveRegister, this);
        publishSchedulerThread_ = std::thread(&OnDemandPub::processPublishTaskScheduler, this);
        freqChangeCbThread_ = std::thread(&OnDemandPub::processFreqChangeCallback, this);

        ONDEMANDLOG(info) << "OnDemandPub started (TimerScheduler: 1ms tick, pool=" << poolSize
                          << ")";
        return true;
    }

    /**
     * @brief 创建 DDS 读写器，设置回调函数
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::createTableDefineWriter()
    {
        constexpr uint32_t depth = 20;
        DdsWrapper::DataWriterQoSBuilder writerQosBuilder;
        writerQosBuilder.setMaxSamples(32 * depth)
            .setMaxInstances(32)
            .setMaxSamplesPerInstance(depth)
            .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
            .setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
            .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
            .setHistoryDepth(depth);
        // .setFlowController("reliable_flow_controller");

        if (0
            != dsf::ondemand::registerNodeTopicWriter<DSF::Var::PubTableDefine,
                                                      DSF::Var::PubTableDefinePubSubType>(
                dataNode_, pubTableDefineWriter_, DSF::Var::TABLE_DEFINE_TOPIC_NAME,
                writerQosBuilder)) {
            ONDEMANDLOG(error) << "Failed to register topic for PubTableDefine: "
                               << DSF::Var::TABLE_DEFINE_TOPIC_NAME;
            return false;
        }

        return true;
    }

    /**
     * @brief 创建订阅注册读取器，设置回调函数处理订阅者注册请求
     * @param  processFunc 数据处理回调函数
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::createSubTableRegisterReader(
        std::function<void(const std::string &, std::shared_ptr<DSF::Message::SubTableRegister>)>
            processFunc)
    {
        constexpr uint32_t depth = 100;
        DdsWrapper::DataReaderQoSBuilder readerQosBuilder;
        readerQosBuilder.setMaxSamples(256 * depth)
            .setMaxInstances(256)
            .setMaxSamplesPerInstance(depth)
            .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
            .setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
            .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
            .setHistoryDepth(depth);

        if (0
            != dsf::ondemand::registerNodeTopicReader<DSF::Message::SubTableRegister,
                                                      DSF::Message::SubTableRegisterPubSubType>(
                dataNode_, subTableRegisterReqReader_,
                DSF::Message::MESSAGE_COMMAND_REQUEST_SUB_TABLE_REGISTER_TOPIC_NAME, processFunc,
                readerQosBuilder)) {
            ONDEMANDLOG(error)
                << "Failed to register topic for SubTableRegister: "
                << DSF::Message::MESSAGE_COMMAND_REQUEST_SUB_TABLE_REGISTER_TOPIC_NAME;
            return false;
        }

        return true;
    }

    /**
     * @brief 创建数据传输写入器，按 bucket 分配，支持分片发布
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::createDataTransferWriter()
    {
        /*根据 varIndex_ 中的元数据，收集所有不同的 bucket id*/
        std::unordered_set<uint32_t> bucketIds;
        {
            std::shared_lock lock(varIndexMutex_);
            for (const auto &[hash, meta] : varIndex_) {
                bucketIds.insert(static_cast<uint32_t>(meta.bucketIndex));
            }
        }

        if (bucketIds.empty()) {
            ONDEMANDLOG(warning) << "No variables registered, no data transfer writers to create.";
            return false;
        }

        DdsWrapper::DataWriterQoSBuilder writerQosBuilder;
        // TODO: 配置 DataTransfer writer QoS
        // writerQosBuilder.setMaxSamples(...)
        //     .setDurabilityKind(...)
        //     .setReliabilityKind(...)
        //     .setHistoryKind(...)
        //     .setHistoryDepth(...);

        std::lock_guard<std::mutex> lock(DataTransferWriterMapMutex_);
        for (uint32_t bucketId : bucketIds) {
            /*如果该 bucket 的 writer 已存在，跳过*/
            if (dataTransferWriterMap_.find(bucketId) != dataTransferWriterMap_.end()) {
                ONDEMANDLOG(debug)
                    << "DataTransfer writer already exists for bucketId: " << bucketId;
                continue;
            }

            std::string tableName = make_bucket_name_by_id(bucketId);
            std::string topicName = DSF::Var::VAR_DATA_TRANSFER_TOPIC_PREFIX + tableName;
            std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Var::TableDataTransfer>> writer;

            writer = dataNode_->createDataWriter<DSF::Var::TableDataTransfer,
                                                 DSF::Var::TableDataTransferPubSubType>(
                topicName, DATA_TANSFER_PUB_SUB_NAME, writerQosBuilder);
            if (!writer) {
                ONDEMANDLOG(error)
                    << "Failed to create topic writer for topic [" << topicName << "].";
                return false;
            }
            dataTransferWriterMap_.emplace(bucketId, writer);
            ONDEMANDLOG(info) << "Created DataTransfer writer for bucketId: " << bucketId
                              << ", topic: " << topicName;
        }

        ONDEMANDLOG(info) << "Created " << dataTransferWriterMap_.size()
                          << " DataTransfer writers in total.";
        return true;
    }

    /**
     * @brief 发布表定义，包含变量定义和频率信息，支持增量更新
     * @param  pubTableDefine 发布的表定义信息，包含变量列表和频率等元数据
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::tableDefinePublish(const DSF::Var::PubTableDefine &pubTableDefine)
    {
        if (pubTableDefineWriter_ != nullptr) {
            return pubTableDefineWriter_->writeMessage(pubTableDefine);
        } else {
            ONDEMANDLOG(error) << "pubTableDefineWriter_ is nullptr";
            return false;
        }
    }

    /**
     * @brief 处理订阅者注册请求，更新订阅者信息和变量频率，触发发布调度
     * @param  topicName 订阅者注册的 DDS 主题名称
     * @param  data 数据
     */
    void OnDemandPub::onReceiveRegisterCb(const std::string & /*topicName*/,
                                          std::shared_ptr<DSF::Message::SubTableRegister> data)
    {
        if (!data) {
            return;
        }
        pubTableDefRegisterQueue_.enqueue(data);
    }
    /**
     * @brief 处理订阅者注册请求
     */
    void OnDemandPub::processReceiveRegister()
    {
        pthread_setname_np(pthread_self(), "PubRegProc");
        std::unordered_map<std::string, uint32_t> retryBackoffMsByNode;
        constexpr uint32_t kMinPartialRetryMs = 200;
        constexpr uint32_t kMinAllMissingRetryMs = 800;
        constexpr uint32_t kMaxRetryMs = 3000;

        while (running_.load(std::memory_order_acquire)) {
            std::shared_ptr<DSF::Message::SubTableRegister> data;
            if (pubTableDefRegisterQueue_.try_dequeue(data)) {
                if (data) {
                    switch (data->msgType()) {
                        case DSF::Message::MSGTYPE::SUB_TABLE_REGISTER: {
                            const std::string nodeName = data->nodeName();
                            uint32_t missing = handleSubscribe(nodeName, data->varFreqs());
                            if (missing > 0) {
                                /*但对同一 node 做指数退避，降低长期缺失时的 CPU 开销。*/
                                const size_t total = data->varFreqs().size();
                                const bool allMissing = (total > 0) && (missing >= total);

                                uint32_t &backoffMs = retryBackoffMsByNode[nodeName];
                                if (backoffMs == 0) {
                                    backoffMs = allMissing ? kMinAllMissingRetryMs : kMinPartialRetryMs;
                                } else {
                                    const uint32_t minBase =
                                        allMissing ? kMinAllMissingRetryMs : kMinPartialRetryMs;
                                    if (backoffMs < minBase) {
                                        backoffMs = minBase;
                                    } else {
                                        backoffMs = std::min(kMaxRetryMs, backoffMs * 2);
                                    }
                                }

                                ONDEMANDLOG_TIME(warning, 5000)
                                    << "SUB_TABLE_REGISTER retry: node=" << nodeName
                                    << " missing=" << missing << "/" << total
                                    << " delayMs=" << backoffMs;

                                std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
                                pubTableDefRegisterQueue_.enqueue(data);
                            } else {
                                retryBackoffMsByNode.erase(nodeName);
                            }
                            break;
                        }
                        case DSF::Message::MSGTYPE::SUB_TABLE_UNREGISTER:
                            retryBackoffMsByNode.erase(data->nodeName());
                            handleUnsubscribe(data->nodeName(), data->varFreqs());
                            break;
                        default:
                            ONDEMANDLOG(error) << "Unknown registered type.";
                            break;
                    }

                    // // [DEBUG] 打印相关变量的 VarMetadata
                    // {
                    //     std::shared_lock dumpLock(varIndexMutex_);
                    //     for (const auto &varFreq : data->varFreqs()) {
                    //         std::string metaName = varFreq.name();
                    //         uint64_t varHash = fast_hash(metaName);
                    //         auto it = varIndex_.find(varHash);
                    //         if (it != varIndex_.end()) {
                    //             it->second.dump(varFreq.name());
                    //         }
                    //     }
                    // }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    /**
     * @brief 发布调度监控线程
     *
     * 增量策略:
     *  频率未变 → 定时器不动，仅刷新组成员列表
     *  新增分组 → 创建定时器
     *  移除分组 → 取消定时器
     *  10w 变量同频 →  20 bucket 天然分散, 无需额外分片
     */
    void OnDemandPub::processPublishTaskScheduler()
    {
        pthread_setname_np(pthread_self(), "PubScheduler");
        constexpr auto kScanInterval = std::chrono::milliseconds(50);

        while (running_.load(std::memory_order_acquire)) {

            std::this_thread::sleep_for(kScanInterval);
            // 只在 varIndex_ 有变更时才重建分组 (dirty flag)
            if (!schedulerDirty_.exchange(false, std::memory_order_acq_rel)) {
                continue;
            }

            //从 varIndex_ 构建期望分组
            using DesiredMap =
                std::unordered_map<PublishGroupKey, std::shared_ptr<std::vector<GroupVarInfo>>,
                                   PublishGroupKeyHash>;

            DesiredMap desired;
            {
                std::shared_lock lock(varIndexMutex_);
                for (const auto &[varHash, meta] : varIndex_) {
                    /*没有订阅则不处理*/
                    if (meta.currentFreq == 0xFFFFFFFF || meta.activeFreqCount == 0) {
                        continue;
                    }
                    PublishGroupKey key{static_cast<uint32_t>(meta.bucketIndex), meta.currentFreq};
                    auto &vec = desired[key];
                    if (!vec) {
                        vec = std::make_shared<std::vector<GroupVarInfo>>();
                    }
                    /*<id+freq, varInfo>  这里面存的就是所有的点按照索引 + 周期相同的放一起*/
                    uint32_t runtimeSize = varStore_.slot_size(meta.varId);
                    if (runtimeSize == 0) {
                        continue;
                    }
                    vec->push_back(GroupVarInfo{varHash, meta.varId, runtimeSize});
                }
            }

            /*预排序: 按 varHash 升序, 与 Roaring64Map 迭代顺序一致*/
            for (auto &[gkey, vec] : desired) {
                if (vec) {
                    std::sort(vec->begin(), vec->end(),
                              [](const GroupVarInfo &a, const GroupVarInfo &b) {
                                  return a.varHash < b.varHash;
                              });
                }
            }

            {
                std::lock_guard<std::mutex> lock(publishGroupsMutex_);
                /*新的一轮扫描发现分配的key分配了定时器，但是现在整个变量都没有这种id或者频率了 就把该定时器干掉*/
                for (auto it = publishGroupTimers_.begin(); it != publishGroupTimers_.end();) {
                    if (desired.find(it->first) == desired.end()) {
                        if (publishScheduler_) {
                            publishScheduler_->Cancel(it->second);
                        }
                        groupMembers_.erase(it->first);
                        groupFlatBufs_.erase(it->first);
                        groupMaskBufs_.erase(it->first);
                        ONDEMANDLOG(debug)
                            << "Removed publish group: bucket=" << it->first.bucketIndex
                            << " freq=" << it->first.freqMs << "ms";
                        it = publishGroupTimers_.erase(it);
                    }
                    /*如果这里有，就不用管了，继续调度就好了*/
                    else {
                        ++it;
                    }
                }

                /*遍历所有的点，保存全局副本，然后找一下有没有该周期的时间轮，没有的话，就创建一个，有的话也不用管*/
                for (auto &[key, members] : desired) {
                    /*始终刷新成员列表，保留已有的 running flag*/
                    groupMembers_[key].members = std::move(members);

                    auto &entryMembers = groupMembers_[key].members;
                    if (entryMembers && !entryMembers->empty()) {
                        roaring::Roaring64Map roar;
                        for (const auto &info : *entryMembers) {
                            roar.add(info.varHash);
                        }
                        roar.runOptimize();
                        roar.shrinkToFit();
                        auto buf = std::make_shared<std::vector<uint8_t>>(roar.getSizeInBytes());
                        roar.write(reinterpret_cast<char *>(buf->data()));
                        groupMaskBufs_[key] = std::move(buf);
                    } else {
                        groupMaskBufs_.erase(key);
                    }

                    /*新增分组创建定时器，已有分组的定时器保持不变*/
                    if (publishGroupTimers_.find(key) == publishGroupTimers_.end()) {
                        uint32_t bucketIdx = key.bucketIndex;
                        uint32_t freqMs = key.freqMs;
                        Tick intervalTicks = static_cast<Tick>(freqMs);
                        /*按 bucket 错开初始延迟；小频率时最小错峰步长为 5 ticks (5ms)*/
                        Tick staggerStep = static_cast<Tick>(freqMs / ONDEMAND_BUCKET_SIZE);
                        constexpr Tick kMinStaggerTicks = 5;
                        if (staggerStep < kMinStaggerTicks) {
                            staggerStep = kMinStaggerTicks;
                        }
                        Tick jitter = static_cast<Tick>(bucketIdx) * staggerStep;
                        auto timer = publishScheduler_->ScheduleRecurring(
                            [this, bucketIdx, freqMs]() { publishGroupData(bucketIdx, freqMs); },
                            intervalTicks + jitter, // 首次延迟错开
                            intervalTicks           // 周期
                        );
                        publishGroupTimers_[key] = timer;
                        ONDEMANDLOG(debug)
                            << "Created publish group: bucket=" << bucketIdx << " freq=" << freqMs
                            << "ms, members=" << groupMembers_[key].members->size();
                    }
                }
            }
        }
    }

    /**
     * @brief 批量发布组内变量数据 (定时器回调, 线程池执行)
     *
     * 性能优化:
     *  groupMembers_ 已按 varHash 预排序, 无需发送时再排序
     *  直接构建 msg, 省去中间 varDataList 容器分配
     *  Roaring64Map 压缩 mask
     *
     * @param bucketIndex  桶索引 (映射到 DDS topic)
     * @param freqMs       频率 (ms), 用于定位分组
     */
    void OnDemandPub::publishGroupData(uint32_t bucketIndex, uint32_t freqMs)
    {
        if (!running_.load(std::memory_order_acquire))
            return;

        /*暂停时跳过发布*/
        if (paused_.load(std::memory_order_acquire))
            return;

        /*获取组成员快照 + running flag + 预计算 mask 快照*/
        std::shared_ptr<std::vector<GroupVarInfo>> members;
        std::shared_ptr<std::atomic<bool>> running;
        std::shared_ptr<std::vector<uint8_t>> precomputedMask;
        {
            std::lock_guard<std::mutex> lock(publishGroupsMutex_);
            PublishGroupKey key{bucketIndex, freqMs};
            auto it = groupMembers_.find(key);
            if (it == groupMembers_.end() || !it->second.members || it->second.members->empty()) {
                return;
            }
            members = it->second.members;
            running = it->second.running;

            auto maskIt = groupMaskBufs_.find(key);
            if (maskIt != groupMaskBufs_.end()) {
                precomputedMask = maskIt->second; // shared_ptr 引用，无复制
            }
        }

        /* 防止并发执行：上次还没发完则跳过本次 */
        if (!running || running->exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        struct RunningGuard {
            std::atomic<bool> &flag;
            ~RunningGuard() { flag.store(false, std::memory_order_release); }
        } guard{*running};

        DSF::Var::TableDataTransfer msg;
        msg.varData().reserve(members->size());

        /*用于记录实际发送的变量 hash（只包含有数据的变量）*/
        roaring::Roaring64Map actualMask;
        size_t skippedCount = 0;

        for (const auto &info : *members) {
            auto handle = varStore_.read_zero_copy(info.varId);
            if (!handle || handle.size() == 0) {
                /*跳过没有数据的变量，避免无效发送和 CPU 浪费*/
                ++skippedCount;
                continue;
            }

            /*只发送有数据的变量，始终记录 hash，保证 mask 与 varData 顺序一致*/
            auto &dst = msg.varData().emplace_back();
            dst.resize(handle.size());
            std::memcpy(dst.data(), handle.ptr(), handle.size());
            actualMask.add(info.varHash);
        }

        /*如果所有变量都没数据，跳过本次发送*/
        if (msg.varData().empty()) {
            ONDEMANDLOG_TIME(warning, 30000) // 30秒打印一次，避免日志刷屏
                << "All " << members->size() << " variables in bucket=" << bucketIndex
                << " freq=" << freqMs << "ms have no data, skipping publish";
            return;
        }

        /* 如果有部分变量被跳过，打印一次警告*/
        if (skippedCount > 0) {
            ONDEMANDLOG_TIME(warning, 30000) // 30秒打印一次
                << "Skipped " << skippedCount << "/" << members->size()
                << " variables without data in bucket=" << bucketIndex << " freq=" << freqMs
                << "ms";
        }

        /*无跳过：直接用预计算 mask，避免重建 Roaring64Map（热路径优化）*/
        if (skippedCount == 0 && precomputedMask && !precomputedMask->empty()) {
            msg.mask() = *precomputedMask;
        } else {
            /*有跳过：用实际发送的变量重建 mask（actualMask 已在循环中完整记录）*/
            actualMask.runOptimize();
            actualMask.shrinkToFit();
            msg.mask().resize(actualMask.getSizeInBytes());
            actualMask.write(reinterpret_cast<char *>(msg.mask().data()));
        }

        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(epoch);
        auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch) - sec;
        msg.timestamp().tv_sec(static_cast<int32_t>(sec.count()));
        msg.timestamp().tv_nsec(static_cast<uint32_t>(nsec.count()));
        msg.blobType(static_cast<DSF::Var::BLOB_TYPE>(blobType_.load(std::memory_order_acquire)));

        /*发送*/
        {
            std::lock_guard<std::mutex> lock(DataTransferWriterMapMutex_);
            auto writerIt = dataTransferWriterMap_.find(bucketIndex);
            if (writerIt == dataTransferWriterMap_.end() || !writerIt->second) {
                ONDEMANDLOG_TIME(warning, 5000) << "No writer for bucketIndex=" << bucketIndex;
                return;
            }
            if (!writerIt->second->writeMessage(msg)) {
                ONDEMANDLOG(error) << "Failed to send batch data for bucket=" << bucketIndex
                                   << " freq=" << freqMs << "ms";
            }
        }
    }

    /**
     * @brief 取消所有发布分组定时器并清空组成员索引
     */
    void OnDemandPub::cancelAllPublishTimers()
    {
        std::lock_guard<std::mutex> lock(publishGroupsMutex_);

        for (auto &[key, timer] : publishGroupTimers_) {
            if (publishScheduler_) {
                publishScheduler_->Cancel(timer);
            }
        }
        publishGroupTimers_.clear();
        groupMembers_.clear();
        groupFlatBufs_.clear();
        groupMaskBufs_.clear();

        ONDEMANDLOG(info) << "All publish group timers canceled";
    }

    /**
     * @brief 频率变化回调派发线程，从队列消费并调用用户回调，与注册处理线程解耦
     */
    void OnDemandPub::processFreqChangeCallback()
    {
        pthread_setname_np(pthread_self(), "FreqChangeCb");
        while (running_.load(std::memory_order_acquire)) {
            std::pair<std::string, uint32_t> item;
            if (freqChangeQueue_.try_dequeue(item)) {
                FreqChangeCallback cb;
                {
                    std::lock_guard<std::mutex> lock(freqChangeCbMutex_);
                    cb = freqChangeCb_;
                }

                // 回调尚未注册时不丢事件，避免 sub 先启动、pub 后注册回调导致首个频率变化丢失
                if (!cb) {
                    freqChangeQueue_.enqueue(item);
                    ONDEMANDLOG(debug)
                        << "FreqChangeCallback not set, re-enqueueing freq change event for node: "
                        << item.first << " freq: " << item.second << "ms";
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                try {
                    cb(item.first, item.second);
                } catch (const std::exception &e) {
                    ONDEMANDLOG(error) << "freqChange callback threw exception: " << e.what();
                } catch (...) {
                    ONDEMANDLOG(error) << "freqChange callback threw unknown exception";
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    /**
     * @brief 创建变量并发布表定义，支持增量更新
     * @param  VarDefines 变量定义列表，包含变量名称、类型、频率等信息
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::createVars(const std::vector<DSF::Var::Define> &VarDefines)
    {
        /*注册变量 + 初始化存储*/
        {
            std::unique_lock lock(varIndexMutex_);
            varIndex_.reserve(varIndex_.size() + VarDefines.size());

            for (const auto &VarDefine : VarDefines) {
                const auto &varName = make_meta_varname(nodeName_, VarDefine.name());
                uint64_t varHash = fast_hash(varName);
                size_t bucketIdx = BucketManager::CalculateBucketIndexFromHash(varHash);

                auto it = varIndex_.find(varHash);
                if (it != varIndex_.end()) {
                    ONDEMANDLOG(warning) << "Variable already exists: " << varName;
                    continue;
                }

                VarMetadata meta;
                meta.varHash = varHash;
                meta.currentFreq = 0xFFFFFFFF;
                meta.activeFreqCount = 0;
                meta.bucketIndex = bucketIdx;
                meta.varDefine = std::make_shared<DSF::Var::Define>(VarDefine);
                meta.realVarName = VarDefine.name();
                uint32_t kVarSize =
                    VarDefine.size() > 0 ? static_cast<uint32_t>(VarDefine.size()) : 32u;
                meta.varId = varStore_.register_var(varHash, kVarSize);
                varIndex_.emplace(varHash, std::move(meta));
                bucketManager_.AddMember(varName, varHash);
            }
        } // 先释放写锁，再 finalize，避免 finalize 内 config_begin 等 active_ops==0
          // 时与持有 OpGuard 的 publishGroupData 形成死锁

        if (!varStore_.finalize()) {
            ONDEMANDLOG(error) << "Failed to finalize variable store";
            return false;
        }

        /*创建 DataTransfer writers */
        if (!createDataTransferWriter()) {
            ONDEMANDLOG(error) << "Failed to create DataTransfer writers";
            return false;
        }

        /*发布 TableDefine */
        uint32_t bucketCount = bucketManager_.GetBucketCount();
        for (uint32_t i = 0; i < bucketCount; ++i) {
            if (bucketManager_.GetBucketSize(i) == 0) {
                continue;
            }

            DSF::Var::PubTableDefine pubTableDefine;
            pubTableDefine.name(make_bucket_name_by_id(i));
            pubTableDefine.nodeName(nodeName_);
            pubTableDefine.description("onDemandPub TableDefine");

            const auto members = bucketManager_.GetBucketMembers(i);
            pubTableDefine.varDefines().reserve(members.size());

            {
                std::shared_lock lock(varIndexMutex_);
                for (const auto &varName : members) {
                    uint64_t varHash = fast_hash(varName);
                    auto it = varIndex_.find(varHash);
                    if (it == varIndex_.end()) {
                        continue;
                    }
                    const auto &meta = it->second;

                    DSF::Var::PubTableVarDefine pubTableVarDefine;
                    DSF::Var::VarRequest varRequest;
                    DSF::Var::Define varDefine;
                    varDefine = *(meta.varDefine);
                    varRequest.varDefine(varDefine);
                    pubTableVarDefine.var(std::move(varRequest));
                    pubTableDefine.varDefines().push_back(std::move(pubTableVarDefine));
                }
            }

            if (!pubTableDefine.varDefines().empty()) {
                tableDefinePublish(pubTableDefine);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ONDEMANDLOG(info) << "Published bucket " << i + 1 << "/" << bucketCount << " with "
                                  << pubTableDefine.varDefines().size() << " variables";
            }
        }

        schedulerDirty_.store(true, std::memory_order_release);
        return true;
    }
    /**
     * @brief 删除变量并发布更新的表定义，支持增量更新
     * @param  varNames 变量名称列表，指定要删除的变量
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::deleteVars(const std::vector<std::string> &varNames)
    {
        /*注销被删变量，记录受影响的 bucket，只重发这些 bucket 的 TableDefine*/
        std::unordered_set<uint32_t> affectedBuckets;
        {
            std::unique_lock lock(varIndexMutex_);
            for (const auto &varName : varNames) {
                uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varName));
                auto it = varIndex_.find(varHash);
                if (it == varIndex_.end()) {
                    ONDEMANDLOG(warning) << "Variable not found: " << varName;
                    continue;
                }
                affectedBuckets.insert(static_cast<uint32_t>(it->second.bucketIndex));
                varStore_.unregister_var(varHash);
                varIndex_.erase(it);
                bucketManager_.RemoveMember(make_meta_varname(nodeName_, varName), varHash);
            }
        }

        schedulerDirty_.store(true, std::memory_order_release);

        /*只重发受影响的 bucket 的 TableDefine，避免误触发其他 bucket 的差分删除*/
        for (uint32_t i : affectedBuckets) {
            DSF::Var::PubTableDefine pubTableDefine;
            pubTableDefine.name(make_bucket_name_by_id(i));
            pubTableDefine.nodeName(nodeName_);
            pubTableDefine.description("onDemandPub TableDefine");

            const auto members = bucketManager_.GetBucketMembers(i);
            pubTableDefine.varDefines().reserve(members.size());

            {
                std::shared_lock lock(varIndexMutex_);
                for (const auto &varName : members) {
                    uint64_t varHash = fast_hash(varName);
                    auto it = varIndex_.find(varHash);
                    if (it == varIndex_.end()) {
                        continue;
                    }
                    const auto &meta = it->second;

                    DSF::Var::PubTableVarDefine pubTableVarDefine;
                    DSF::Var::VarRequest varRequest;
                    DSF::Var::Define varDefine;
                    varDefine = *(meta.varDefine);
                    varRequest.varDefine(varDefine);
                    pubTableVarDefine.var(std::move(varRequest));
                    pubTableDefine.varDefines().push_back(std::move(pubTableVarDefine));
                }
            }

            /*差分删除: 即使是空表也要发布, 通知订阅者该表已清空*/
            tableDefinePublish(pubTableDefine);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ONDEMANDLOG(info) << "Published bucket " << i << " with "
                              << pubTableDefine.varDefines().size() << " variables (delete mode)";
        }

        return true;
    }
    /**
     * @brief 设置变量数据，支持增量更新和分片发布
     * @param  varName 变量名称，必须已创建
     * @param  data 变量数据指针，指向外部数据源
     * @param  size 数据大小，单位字节
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::setVarData(const char *varName, const void *data, size_t size)
    {
        uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varName));

        uint32_t varId = VarStore::kInvalidId;
        {
            std::shared_lock lock(varIndexMutex_);
            auto it = varIndex_.find(varHash);
            if (it == varIndex_.end()) {
                return false;
            }
            varId = it->second.varId;
        }
        /*写数据*/
        if (!varStore_.write(varId, data, size)) {
            ONDEMANDLOG(error) << "Failed to set data for variable: " << varName;
            return false;
        }
        return true;
    }

    uint32_t OnDemandPub::getVarId(const char *varName) const
    {
        uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varName));
        std::shared_lock lock(varIndexMutex_);
        auto it = varIndex_.find(varHash);
        if (it == varIndex_.end()) {
            ONDEMANDLOG(warning) << "Variable not found: " << (varName ? varName : "nullptr");
            return UINT32_MAX;
        }
        return it->second.varId;
    }

    void OnDemandPub::getVarIds(const char *const *names, uint32_t *ids, size_t count) const
    {
        if (ids == nullptr || names == nullptr) {
            return;
        }

        std::shared_lock lock(varIndexMutex_);
        for (size_t i = 0; i < count; ++i) {
            if (names[i] == nullptr) {
                ids[i] = UINT32_MAX;
                continue;
            }
            uint64_t varHash = fast_hash(make_meta_varname(nodeName_, names[i]));
            auto it = varIndex_.find(varHash);
            ids[i] = (it != varIndex_.end()) ? it->second.varId : UINT32_MAX;
        }
    }

    void OnDemandPub::setVarData(uint32_t varId, const void *data, size_t size)
    {
        if (data == nullptr || size == 0) {
            return;
        }
        uint32_t writeSize =
            static_cast<uint32_t>(size > static_cast<size_t>(UINT32_MAX) ? UINT32_MAX : size);
        if (size > static_cast<size_t>(UINT32_MAX)) {
            ONDEMANDLOG_TIME(warning, 5000)
                << "setVarData: size overflowed uint32_t and was clipped, varId=" << varId;
        }
        varStore_.write(varId, data, writeSize);
    }

    void OnDemandPub::setVarDataBatch(const VarWriteItem *items, size_t count)
    {
        constexpr size_t kStackMax = 4096;

        if (items == nullptr || count == 0) {
            return;
        }

        uint32_t invalidCount = 0;
        uint32_t clippedSizeCount = 0;

        auto run = [&](uint32_t *ids, const void **datas, uint32_t *sizes) {
            for (size_t i = 0; i < count; ++i) {
                ids[i] = items[i].id;
                datas[i] = items[i].data;
                if (items[i].size > static_cast<size_t>(UINT32_MAX)) {
                    sizes[i] = UINT32_MAX;
                    ++clippedSizeCount;
                } else {
                    sizes[i] = static_cast<uint32_t>(items[i].size);
                }
                if (items[i].id == UINT32_MAX || items[i].data == nullptr) {
                    ++invalidCount;
                }
            }
            varStore_.write_batch(ids, datas, sizes, count);
        };

        if (count <= kStackMax) {
            uint32_t ids[kStackMax];
            const void *datas[kStackMax];
            uint32_t sizes[kStackMax];
            run(ids, datas, sizes);
        } else {
            std::vector<uint32_t> ids(count);
            std::vector<const void *> datas(count);
            std::vector<uint32_t> sizes(count);
            run(ids.data(), datas.data(), sizes.data());
        }

        if (invalidCount > 0) {
            ONDEMANDLOG_TIME(warning, 5000) << "setVarDataBatch: " << invalidCount << " / " << count
                                            << " items have invalid varId or null data";
        }
        if (clippedSizeCount > 0) {
            ONDEMANDLOG_TIME(warning, 5000) << "setVarDataBatch: " << clippedSizeCount
                                            << " items size overflowed uint32_t and were clipped";
        }
    }

    /**
     * @brief 获取或分配订阅者节点的位图位置，支持最多 64 个订阅者
     * @param  nodeHash 订阅者节点名称的 hash 值
     * @return uint8_t 节点位图位置
     */
    uint8_t OnDemandPub::getOrAssignNodeBit(uint64_t nodeHash)
    {
        auto it = nodeSlotMap_.find(nodeHash);
        if (it != nodeSlotMap_.end()) {
            return it->second;
        }
        if (nextNodeSlot_ >= 64) {
            /* 超过 64 个订阅节点：返回 kInvalidSlot，调用方需检查并拒绝该订阅，
             * 避免多节点共享同一 bit 导致 unsubscribe 时错误清除其他节点的订阅 */
            ONDEMANDLOG(error) << "Node slot overflow! Max 64 subscriber nodes supported."
                               << " nodeHash=" << nodeHash;
            return 0xFF; // kInvalidNodeSlot
        }
        uint8_t slot = nextNodeSlot_++;
        nodeSlotMap_.emplace(nodeHash, slot);
        return slot;
    }

    void OnDemandPub::recalcCurrentFreq(VarMetadata &meta)
    {
        uint32_t minFreq = 0xFFFFFFFF;
        for (const auto &fs : meta.freqSubs) {
            if (fs.subCount > 0 && fs.freq < minFreq) {
                minFreq = fs.freq;
            }
        }
        meta.currentFreq = minFreq;
    }

    /**
     * @brief 处理订阅者注册请求，更新订阅者信息和变量频率，触发发布调度
     * @param  nodeName 变量名
     * @param  varFreqs 周期
     */
    uint32_t OnDemandPub::handleSubscribe(const std::string &nodeName,
                                          const std::vector<DSF::NamedValue> &varFreqs)
    {
        uint64_t nodeHash = fast_hash(nodeName);

        std::vector<std::pair<std::string, uint32_t>> freqChanges; // 锁外触发回调
        uint32_t missingCount = 0;

        std::unique_lock lock(varIndexMutex_);
        uint8_t nodeBit = getOrAssignNodeBit(nodeHash);
        if (nodeBit == 0xFF) {
            ONDEMANDLOG(error) << "Rejecting subscribe from node=" << nodeName
                               << ": exceeded max 64 subscriber nodes";
            return 0;
        }
        uint64_t nodeMask = uint64_t(1) << nodeBit;

        for (const auto &varFreq : varFreqs) {
            std::string metaName = varFreq.name(); // 这里注册请求已经是全名了，不需要再拼接一次了
            uint64_t varHash = fast_hash(metaName);
            auto it = varIndex_.find(varHash);
            if (it == varIndex_.end()) {
                // Sub 先 subscribe()、Pub 后 createVars() 的正常竞态，调用方会延迟重试
                ONDEMANDLOG(debug) << "handleSubscribe: var not yet created, will retry"
                                   << " var=" << varFreq.name() << " node=" << nodeName;
                ++missingCount;
                continue;
            }

            auto &meta = it->second;

            /*解析频率*/
            uint32_t freq;
            auto result = std::from_chars(varFreq.value().data(),
                                          varFreq.value().data() + varFreq.value().size(), freq);
            if (std::errc() != result.ec) {
                ONDEMANDLOG(warning) << "Invalid frequency value for var: " << varFreq.name()
                                     << " node: " << nodeName << " value: " << varFreq.value();
                continue;
            }

            /*先从该节点已有的其他频率条目中移除*/
            for (auto fsIt = meta.freqSubs.begin(); fsIt != meta.freqSubs.end();) {
                if (fsIt->subMask & nodeMask) {
                    fsIt->subMask &= ~nodeMask;
                    fsIt->subCount--;
                    if (fsIt->subCount == 0) {
                        fsIt = meta.freqSubs.erase(fsIt);
                    } else {
                        ++fsIt;
                    }
                } else {
                    ++fsIt;
                }
            }

            /*在 freqSubs 中找到匹配 freq 的条目，或新建*/
            VarMetadata::FreqSub *target = nullptr;
            for (auto &fs : meta.freqSubs) {
                if (fs.freq == freq) {
                    target = &fs;
                    break;
                }
            }
            if (!target) {
                meta.freqSubs.emplace_back();
                target = &meta.freqSubs.back();
                target->freq = freq;
            }

            /*设置该节点的订阅位*/
            target->subMask |= nodeMask;
            target->subCount++;

            /*更新活跃频率数量*/
            meta.activeFreqCount = static_cast<uint8_t>(meta.freqSubs.size());

            /*重新计算最小发布频率*/
            uint32_t oldFreq = meta.currentFreq;
            recalcCurrentFreq(meta);
            schedulerDirty_.store(true, std::memory_order_release);

            if (meta.currentFreq != oldFreq) {
                freqChanges.emplace_back(meta.realVarName, meta.currentFreq);
            }

            ONDEMANDLOG(info) << "Var [" << varFreq.name() << "] subscribed by node [" << nodeName
                              << "] at freq=" << freq << "ms, currentFreq=" << meta.currentFreq
                              << "ms";
        }

        lock.unlock();
        for (const auto &[varName, newFreq] : freqChanges) {
            freqChangeQueue_.enqueue({varName, newFreq});
        }
        return missingCount;
    }

    /**
     * @brief 处理订阅者取消订阅请求，更新订阅者信息和变量频率，触发发布调度
     * @param  nodeName 变量名
     * @param  varFreqs 周期
     */
    void OnDemandPub::handleUnsubscribe(const std::string &nodeName,
                                        const std::vector<DSF::NamedValue> &varFreqs)
    {
        uint64_t nodeHash = fast_hash(nodeName);

        std::vector<std::pair<std::string, uint32_t>> freqChanges; // 锁外触发回调

        std::unique_lock lock(varIndexMutex_);

        /*查找该节点的 bit 位*/
        auto slotIt = nodeSlotMap_.find(nodeHash);
        if (slotIt == nodeSlotMap_.end()) {
            ONDEMANDLOG(warning) << "Unsubscribe from unknown node: " << nodeName;
            return;
        }
        uint64_t nodeMask = uint64_t(1) << slotIt->second;

        for (const auto &varFreq : varFreqs) {
            std::string metaName = varFreq.name();
            uint64_t varHash = fast_hash(metaName);
            auto it = varIndex_.find(varHash);
            if (it == varIndex_.end()) {
                ONDEMANDLOG(warning) << "unregister var not found ! var name: " << varFreq.name()
                                     << " node name: " << nodeName;
                continue;
            }

            auto &meta = it->second;

            /*从 freqSubs 中移除该节点的订阅*/
            for (auto fsIt = meta.freqSubs.begin(); fsIt != meta.freqSubs.end();) {
                if (fsIt->subMask & nodeMask) {
                    fsIt->subMask &= ~nodeMask;
                    fsIt->subCount--;
                    if (fsIt->subCount == 0) {
                        fsIt = meta.freqSubs.erase(fsIt);
                    } else {
                        ++fsIt;
                    }
                } else {
                    ++fsIt;
                }
            }

            /*更新活跃频率数量*/
            meta.activeFreqCount = static_cast<uint8_t>(meta.freqSubs.size());

            /*重新计算最小频率*/
            uint32_t oldFreq = meta.currentFreq;
            recalcCurrentFreq(meta);
            schedulerDirty_.store(true, std::memory_order_release);

            if (meta.currentFreq != oldFreq) {
                freqChanges.emplace_back(meta.realVarName, meta.currentFreq);
            }

            ONDEMANDLOG(info) << "Var [" << varFreq.name() << "] unsubscribed by node [" << nodeName
                              << "], freq " << oldFreq << "ms -> " << meta.currentFreq << "ms";
        }

        lock.unlock();
        for (const auto &[varName, newFreq] : freqChanges) {
            freqChangeQueue_.enqueue({varName, newFreq});
        }
    }

    /**
     * @brief 订阅者发现回调，处理新订阅者的注册信息，更新内部状态
     * @param  info 订阅者端点信息，包含节点名称、订阅的变量和频率等
     */
    void OnDemandPub::onWriterDiscovery(const DdsWrapper::EndpointInfo &info)
    {
        ONDEMANDLOG(debug) << "[pub node]Writer discovery: topic=" << info.topic_name
                           << " type=" << info.type_name << " discovered=" << info.discovered;
    }

    void OnDemandPub::onParticipantDiscovery(const DdsWrapper::ParticipantInfo &info)
    {
        if (info.status != DdsWrapper::ParticipantStatus::REMOVED
            && info.status != DdsWrapper::ParticipantStatus::DROPPED) {
            return;
        }

        (void)cleanupParticipantSubscriptions(info.participant_name);
    }

    bool OnDemandPub::cleanupParticipantSubscriptions(const std::string &participantName)
    {
        if (participantName.empty()) {
            ONDEMANDLOG(warning)
                << "cleanupParticipantSubscriptions failed: empty participant name";
            return false;
        }

        uint64_t nodeHash = fast_hash(participantName);

        std::unique_lock lock(varIndexMutex_);

        auto slotIt = nodeSlotMap_.find(nodeHash);
        if (slotIt == nodeSlotMap_.end()) {
            ONDEMANDLOG(debug) << "cleanupParticipantSubscriptions skipped: participant not found: "
                               << participantName;
            return false;
        }

        uint64_t nodeMask = uint64_t(1) << slotIt->second;
        auto freqChanges = forceUnsubscribeNode(nodeMask);

        nodeSlotMap_.erase(slotIt);

        lock.unlock();

        ONDEMANDLOG(info) << "Participant cleanup: " << participantName << ", force-unsubscribed "
                          << freqChanges.size() << " vars";

        for (const auto &[varName, newFreq] : freqChanges) {
            freqChangeQueue_.enqueue({varName, newFreq});
        }

        return true;
    }

    std::vector<std::pair<std::string, uint32_t>>
    OnDemandPub::forceUnsubscribeNode(uint64_t nodeMask)
    {
        std::vector<std::pair<std::string, uint32_t>> freqChanges;

        for (auto &[varHash, meta] : varIndex_) {
            bool changed = false;
            for (auto fsIt = meta.freqSubs.begin(); fsIt != meta.freqSubs.end();) {
                if (fsIt->subMask & nodeMask) {
                    fsIt->subMask &= ~nodeMask;
                    fsIt->subCount--;
                    if (fsIt->subCount == 0) {
                        fsIt = meta.freqSubs.erase(fsIt);
                    } else {
                        ++fsIt;
                    }
                    changed = true;
                } else {
                    ++fsIt;
                }
            }
            if (changed) {
                meta.activeFreqCount = static_cast<uint8_t>(meta.freqSubs.size());
                uint32_t oldFreq = meta.currentFreq;
                recalcCurrentFreq(meta);
                schedulerDirty_.store(true, std::memory_order_release);
                if (meta.currentFreq != oldFreq) {
                    // [P1-3] 直接使用 realVarName，移除从未使用的 fallbackName
                    freqChanges.emplace_back(meta.realVarName, meta.currentFreq);
                }
            }
        }

        return freqChanges;
    }

    /**
     * @brief 停止发布者节点，清理资源，停止所有定时器和线程
     */
    void OnDemandPub::stop()
    {
        initialized_.store(false);
        bool wasRunning = running_.exchange(false);

        if (wasRunning) {
            /*先阻止新增定时发布，缩小 stop 后继续发包窗口*/
            cancelAllPublishTimers();
            if (publishScheduler_) {
                publishScheduler_->Stop();
            }

            /*等待线程退出*/
            if (registerProcessThread_.joinable()) {
                registerProcessThread_.join();
            }
            if (publishSchedulerThread_.joinable()) {
                publishSchedulerThread_.join();
            }
            if (freqChangeCbThread_.joinable()) {
                freqChangeCbThread_.join();
            }

            if (publishScheduler_) {
                publishScheduler_.reset();
            }
        }

        /*清理 varIndex_、bucketManager_、nodeSlotMap_ (受 varIndexMutex_ 保护)*/
        {
            std::unique_lock lock(varIndexMutex_);
            varIndex_.clear();
            bucketManager_.Clear();
            nodeSlotMap_.clear();
            nextNodeSlot_ = 0;
        }

        /*清理发布分组相关数据*/
        {
            std::lock_guard<std::mutex> lock(publishGroupsMutex_);
            publishGroupTimers_.clear();
            groupMembers_.clear();
            groupFlatBufs_.clear();
            groupMaskBufs_.clear();
        }

        /*清理 DDS 读写器和节点*/
        pubTableDefineWriter_.reset();
        subTableRegisterReqReader_.reset();
        {
            std::lock_guard<std::mutex> lock(DataTransferWriterMapMutex_);
            dataTransferWriterMap_.clear();
        }
        dataNode_.reset();
        dataNode_ = nullptr;

        std::shared_ptr<DSF::Message::SubTableRegister> item;
        while (pubTableDefRegisterQueue_.try_dequeue(item)) {
        }

        size_t drainedFreqChange = 0;
        std::pair<std::string, uint32_t> freqChangeItem;
        while (freqChangeQueue_.try_dequeue(freqChangeItem)) {
        }

        ONDEMANDLOG(info) << "OnDemandPub stopped";
    }

} // namespace ondemand
} // namespace dsf
