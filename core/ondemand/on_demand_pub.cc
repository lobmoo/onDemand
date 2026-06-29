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
          subTableRegisterReqReader_(nullptr), freqChangeCb_(nullptr),
          maxVarsPerNode_(0), maxNodeNum_(64)
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

        if (!dataNode_->createPublisher(DATA_TRANSFER_PUB_SUB_NAME, pubQos)) {
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

        /*创建时间轮调度器: 1ms tick，thread_pool_size=0 表示回调直接在 timer 线程执行
         * publishGroupData 是纯 CPU（read_batch + DDS write），无用户代码，不需要线程池*/
        publishScheduler_ = std::make_unique<TimerScheduler>(1, 0);
        registerProcessThread_ = std::thread(&OnDemandPub::processReceiveRegister, this);
        publishSchedulerThread_ = std::thread(&OnDemandPub::processPublishTaskScheduler, this);
        freqChangeCbThread_ = std::thread(&OnDemandPub::processFreqChangeCallback, this);

        ONDEMANDLOG(info) << "OnDemandPub started (TimerScheduler: 1ms tick, direct execute)";
        return true;
    }

    /**
     * @brief 创建 DDS 读写器，设置回调函数
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::createTableDefineWriter()
    {
        constexpr uint32_t depth = 1;
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
                writerQosBuilder, this)) {
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
        constexpr uint32_t depth = 64;
        DdsWrapper::DataReaderQoSBuilder readerQosBuilder;
        readerQosBuilder.setMaxSamples(10 * depth)
            .setMaxInstances(10)
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
                readerQosBuilder,
                createReaderListener<DSF::Message::SubTableRegister>())) {
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
        writerQosBuilder.setAsyncPublisherMode(true);
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
                topicName, DATA_TRANSFER_PUB_SUB_NAME, writerQosBuilder, this);
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

            /*优先消费正常队列（新消息），空了再消费重试队列*/
            if (!pubTableDefRegisterQueue_.try_dequeue(data)) {
                std::lock_guard<std::mutex> lk(pubTableDefRetryQueueMutex_);
                if (!pubTableDefRetryQueue_.empty()) {
                    data = pubTableDefRetryQueue_.front();
                    pubTableDefRetryQueue_.pop();
                }
            }

            if (!data) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            switch (data->msgType()) {
                case DSF::Message::MSGTYPE::SUB_TABLE_REGISTER: {
                    const std::string nodeName = data->nodeName();
                    uint32_t missing = handleSubscribe(nodeName, data->varFreqs());
                    if (missing > 0) {
                        /*指数退避，不丢弃，持续重试直到变量被创建*/
                        const size_t total = data->varFreqs().size();
                        const bool allMissing = (total > 0) && (missing >= total);

                        uint32_t &backoffMs = retryBackoffMsByNode[nodeName];
                        if (backoffMs == 0) {
                            backoffMs =
                                allMissing ? kMinAllMissingRetryMs : kMinPartialRetryMs;
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

                        /*构造只含缺失变量的重试消息，放入重试队列*/
                        auto retryData = std::make_shared<DSF::Message::SubTableRegister>();
                        retryData->msgType(data->msgType());
                        retryData->nodeName(data->nodeName());
                        retryData->tableName(data->tableName());
                        for (const auto &vf : data->varFreqs()) {
                            retryData->varFreqs().push_back(vf);
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
                        {
                            std::lock_guard<std::mutex> lk(pubTableDefRetryQueueMutex_);
                            pubTableDefRetryQueue_.push(retryData);
                        }
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

        /*批量读：一次 op_enter 覆盖所有变量，避免 N 次独立锁开销*/
        const size_t n = members->size();
        std::vector<uint32_t> ids(n);
        for (size_t i = 0; i < n; ++i) {
            ids[i] = (*members)[i].varId;
        }     
        varStore_.read_batch(ids.data(), n, [&](size_t i, const void *ptr, uint32_t sz) {
            if (!ptr || sz == 0) {
                if (skippedCount == 0) {
                    for (size_t j = 0; j < i; ++j)
                        actualMask.add((*members)[j].varHash);
                }
                ONDEMANDLOG_TIME(warning, 3000)
                    << "Variable has no data: varHash=" << (*members)[i].varHash
                    << " varId=" << (*members)[i].varId << " bucket=" << bucketIndex
                    << " freq=" << freqMs << "ms";
                ++skippedCount;
                return;
            }
            auto &dst = msg.varData().emplace_back();
            dst.resize(sz);
            std::memcpy(dst.data(), ptr, sz);
            if (skippedCount > 0)
                actualMask.add((*members)[i].varHash);
        });

        /*如果所有变量都没数据，跳过本次发送*/
        if (msg.varData().empty()) {
            ONDEMANDLOG_TIME(warning, 3000)
                << "All " << members->size() << " variables in bucket=" << bucketIndex
                << " freq=" << freqMs << "ms have no data, skipping publish";
            return;
        }

        /* 如果有部分变量被跳过，打印一次警告*/
        if (skippedCount > 0) {
            ONDEMANDLOG_TIME(warning, 3000) << "Skipped " << skippedCount << "/" << members->size()
                                             << " variables without data in bucket=" << bucketIndex
                                             << " freq=" << freqMs << "ms";
        }

        /*无跳过：直接用预计算 mask，避免重建 Roaring64Map（热路径优化）*/
        if (skippedCount == 0 && precomputedMask && !precomputedMask->empty()) {
            msg.mask() = *precomputedMask;
        } else {
            /*有跳过：用实际发送的变量重建 mask*/
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
        // ── 第一步：预计算 hash + bucketIdx，一次性收集 ──
        struct NewVar {
            std::string varName;
            uint64_t varHash;
            uint32_t bucketIdx;
            const DSF::Var::Define *define; // 指向输入参数，不拷贝
        };
        std::vector<NewVar> newVars;
        newVars.reserve(VarDefines.size());

        for (const auto &VarDefine : VarDefines) {
            std::string varName = make_meta_varname(nodeName_, VarDefine.name());
            uint64_t varHash = fast_hash(varName);
            uint32_t bucketIdx =
                static_cast<uint32_t>(BucketManager::CalculateBucketIndexFromHash(varHash));
            newVars.push_back({std::move(varName), varHash, bucketIdx, &VarDefine});
        }

        // ── 第二步：去重 + 批量注册 + 填充索引（一次写锁 + 一次 ConfigGuard）──
        std::unordered_set<uint32_t> affectedBuckets;
        {
            std::unique_lock lock(varIndexMutex_);
            varIndex_.reserve(varIndex_.size() + newVars.size());
            defineLookup_.reserve(defineLookup_.size() + newVars.size());

            // 去重
            size_t writeIdx = 0;
            for (size_t i = 0; i < newVars.size(); ++i) {
                if (varIndex_.find(newVars[i].varHash) != varIndex_.end()) {
                    ONDEMANDLOG(warning) << "Variable already exists: " << newVars[i].varName;
                    continue;
                }
                if (writeIdx != i)
                    newVars[writeIdx] = std::move(newVars[i]);
                ++writeIdx;
            }
            newVars.resize(writeIdx);

            if (newVars.empty())
                return true;

            // 批量注册（一次 ConfigGuard）
            std::vector<uint64_t> hashes(newVars.size());
            std::vector<uint32_t> sizes(newVars.size(), 0); //暂时不支持预设大小，统一传 0 由 VarStore 内部处理 优化内存
            std::vector<uint32_t> ids(newVars.size());
            for (size_t i = 0; i < newVars.size(); ++i)
                hashes[i] = newVars[i].varHash;
            varStore_.register_var_batch(hashes.data(), sizes.data(), ids.data(), newVars.size());

            // 批量填充
            for (size_t i = 0; i < newVars.size(); ++i) {
                VarMetadata meta;
                meta.currentFreq = 0xFFFFFFFF;
                meta.activeFreqCount = 0;
                meta.bucketIndex = newVars[i].bucketIdx;
                meta.realVarName = newVars[i].define->name();
                meta.varId = ids[i];
                defineLookup_[newVars[i].varHash] = static_cast<uint32_t>(defineCache_.size());
                defineCache_.push_back(*newVars[i].define);
                varIndex_.emplace(newVars[i].varHash, std::move(meta));
                bucketManager_.AddMember(newVars[i].varHash);
                affectedBuckets.insert(newVars[i].bucketIdx);
            }
        } // 释放写锁，再 finalize

        if (!varStore_.finalize()) {
            ONDEMANDLOG(error) << "Failed to finalize variable store";
            return false;
        }

        if (!createDataTransferWriter()) {
            ONDEMANDLOG(error) << "Failed to create DataTransfer writers";
            return false;
        }

        // ── 第三步：发布 PubTableDefine（一次 shared_lock 覆盖所有 bucket）──
        {
            std::shared_lock lock(varIndexMutex_);
            for (uint32_t bucketId : affectedBuckets) {
                if (bucketManager_.GetBucketSize(bucketId) == 0)
                    continue;

                DSF::Var::PubTableDefine pubTableDefine;
                pubTableDefine.name(make_bucket_name_by_id(bucketId));
                pubTableDefine.nodeName(nodeName_);
                pubTableDefine.description("onDemandPub TableDefine");

                const auto &members = bucketManager_.GetBucketMembers(bucketId);
                pubTableDefine.varDefines().reserve(members.size() + 32);

                // 已创建的变量（从 defineCache_ 取完整 Define）
                for (uint64_t varHash : members) {
                    auto idxIt = defineLookup_.find(varHash);
                    if (idxIt == defineLookup_.end())
                        continue;

                    DSF::Var::PubTableVarDefine pubTableVarDefine;
                    DSF::Var::VarRequest varRequest;
                    varRequest.varDefine(defineCache_[idxIt->second]);
                    pubTableVarDefine.var(std::move(varRequest));
                    pubTableDefine.varDefines().push_back(std::move(pubTableVarDefine));
                }

                // 仅注册未创建的变量（从 liteBucketMembers_ 取，构造最小 Define）
                // 避免 sub 端差分删除误删这些变量
                {
                    std::shared_lock liteLock(liteVarIndexMutex_);
                    for (const auto &entry : liteBucketMembers_[bucketId].entries) {
                        if (varIndex_.count(entry.hash))
                            continue; // 已在上面处理
                        const char *nameStr = namePool_.data() + entry.nameOffset;
                        DSF::Var::Define liteDefine;
                        liteDefine.name(nameStr);
                        liteDefine.nodeName(nodeName_);

                        DSF::Var::PubTableVarDefine pubTableVarDefine;
                        DSF::Var::VarRequest varRequest;
                        varRequest.varDefine(std::move(liteDefine));
                        pubTableVarDefine.var(std::move(varRequest));
                        pubTableDefine.varDefines().push_back(std::move(pubTableVarDefine));
                    }
                }

                if (!pubTableDefine.varDefines().empty()) {
                    tableDefinePublish(pubTableDefine);
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    ONDEMANDLOG(info) << "Published bucket " << bucketId << " with "
                                      << pubTableDefine.varDefines().size() << " variables";
                }
            }
        }

        schedulerDirty_.store(true, std::memory_order_release);
        return true;
    }

    bool OnDemandPub::registerVars(const std::vector<DSF::Var::Define> &VarDefines)
    {
        std::unordered_set<uint32_t> affectedBuckets;

        // 1. 提取 name + size，按 bucket 分组存入 liteBucketMembers_
        {
            std::unique_lock lock(liteVarIndexMutex_);
            for (const auto &def : VarDefines) {
                std::string metaName = make_meta_varname(nodeName_, def.name());
                uint64_t varHash = fast_hash(metaName);
                uint32_t bucketIdx =
                    static_cast<uint32_t>(BucketManager::CalculateBucketIndexFromHash(varHash));

                auto &bucket = liteBucketMembers_[bucketIdx];
                if (!bucket.hashSet.insert(varHash).second)
                    continue;

                uint32_t nameOffset = static_cast<uint32_t>(namePool_.size());
                const std::string &name = def.name();
                namePool_.insert(namePool_.end(), name.begin(), name.end());
                namePool_.push_back('\0');

                bucket.entries.push_back(LiteVarEntry{varHash, nameOffset});
                affectedBuckets.insert(bucketIdx);
            }

        }

        // 2. 广播 PubTableDefine（从 liteBucketMembers_ 构造最小 Define）
        if (!affectedBuckets.empty())
            broadcastLiteTableDefines(affectedBuckets);

        ONDEMANDLOG(info) << "registerVars: done, " << VarDefines.size() << " vars, "
                          << affectedBuckets.size() << " buckets";
        return true;
    }

    void OnDemandPub::broadcastLiteTableDefines(const std::unordered_set<uint32_t> &bucketIds,
                                                bool forceEmpty)
    {
        std::shared_lock lock(liteVarIndexMutex_);
        for (uint32_t bucketId : bucketIds) {
            const auto &members = liteBucketMembers_[bucketId].entries;
            if (members.empty() && !forceEmpty)
                continue;

            DSF::Var::PubTableDefine pubTableDefine;
            pubTableDefine.name(make_bucket_name_by_id(bucketId));
            pubTableDefine.nodeName(nodeName_);
            pubTableDefine.description("onDemandPub TableDefine");
            pubTableDefine.varDefines().reserve(members.size());

            for (const auto &entry : members) {
                const char *nameStr = namePool_.data() + entry.nameOffset;

                // 构造最小 Define（仅 name/nodeName，足够 sub 注册）
                DSF::Var::Define define;
                define.name(nameStr);
                define.nodeName(nodeName_);

                DSF::Var::PubTableVarDefine pubTableVarDefine;
                DSF::Var::VarRequest varRequest;
                varRequest.varDefine(std::move(define));
                pubTableVarDefine.var(std::move(varRequest));
                pubTableDefine.varDefines().push_back(std::move(pubTableVarDefine));
            }

            tableDefinePublish(pubTableDefine);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ONDEMANDLOG(info) << "Published bucket " << bucketId << " with "
                              << members.size() << " vars (lite)";
        }
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
                defineLookup_.erase(varHash);
                varIndex_.erase(it);
                bucketManager_.RemoveMember(varHash);
            }
        }

        schedulerDirty_.store(true, std::memory_order_release);

        /*只重发受影响的 bucket 的 TableDefine，避免误触发其他 bucket 的差分删除*/
        for (uint32_t i : affectedBuckets) {
            DSF::Var::PubTableDefine pubTableDefine;
            pubTableDefine.name(make_bucket_name_by_id(i));
            pubTableDefine.nodeName(nodeName_);
            pubTableDefine.description("onDemandPub TableDefine");

            const auto &members = bucketManager_.GetBucketMembers(i);
            pubTableDefine.varDefines().reserve(members.size());

            {
                std::shared_lock lock(varIndexMutex_);
                for (uint64_t varHash : members) {
                    auto it = varIndex_.find(varHash);
                    if (it == varIndex_.end()) {
                        continue;
                    }
                    const auto &meta = it->second;
                    (void)meta; // bucketIndex 等字段已在上方 affectedBuckets 中使用
                    auto idxIt = defineLookup_.find(varHash);
                    if (idxIt == defineLookup_.end()) {
                        continue;
                    }

                    DSF::Var::PubTableVarDefine pubTableVarDefine;
                    DSF::Var::VarRequest varRequest;
                    varRequest.varDefine(defineCache_[idxIt->second]);
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
    WriteResult OnDemandPub::setVarData(const char *varName, const void *data, size_t size)
    {
        if (data == nullptr || size == 0) {
            return WriteResult::FAILED;
        }

        uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varName));

        uint32_t varId = VarStore::kInvalidId;
        {
            std::shared_lock lock(varIndexMutex_);
            auto it = varIndex_.find(varHash);
            if (it == varIndex_.end()) {
                return WriteResult::FAILED;
            }
            varId = it->second.varId;
        }

        uint32_t writeSize =
            static_cast<uint32_t>(size > static_cast<size_t>(UINT32_MAX) ? UINT32_MAX : size);
        if (size > static_cast<size_t>(UINT32_MAX)) {
            ONDEMANDLOG_TIME(warning, 5000)
                << "setVarData: size overflowed uint32_t and was clipped, varName=" << varName;
        }

        return varStore_.write(varId, data, writeSize);
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

    WriteResult OnDemandPub::setVarData(uint32_t varId, const void *data, size_t size)
    {
        if (data == nullptr || size == 0) {
            return WriteResult::FAILED;
        }
        uint32_t writeSize =
            static_cast<uint32_t>(size > static_cast<size_t>(UINT32_MAX) ? UINT32_MAX : size);
        if (size > static_cast<size_t>(UINT32_MAX)) {
            ONDEMANDLOG_TIME(warning, 5000)
                << "setVarData: size overflowed uint32_t and was clipped, varId=" << varId;
        }
        return varStore_.write(varId, data, writeSize);
    }

    WriteResult OnDemandPub::setVarDataBatch(const VarWriteItem *items, size_t count)
    {
        constexpr size_t kStackMax = 4096;

        if (items == nullptr || count == 0) {
            return WriteResult::FAILED;
        }

        uint32_t invalidCount = 0;
        uint32_t clippedSizeCount = 0;

        auto run = [&](uint32_t *ids, const void **datas, uint32_t *sizes) -> WriteResult {
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
                    continue;
                }
                /*已分配但不够大时扩容（size==0 的未分配项由 write_batch 返回 NOT_READY）*/
                uint32_t slotSz = varStore_.slot_size(ids[i]);
                if (slotSz > 0 && slotSz < sizes[i]) {
                    varStore_.ensure_capacity(ids[i], sizes[i]);
                }
            }
            return varStore_.write_batch(ids, datas, sizes, count);
        };

        WriteResult result = WriteResult::FAILED;
        if (count <= kStackMax) {
            uint32_t ids[kStackMax];
            const void *datas[kStackMax];
            uint32_t sizes[kStackMax];
            result = run(ids, datas, sizes);
        } else {
            std::vector<uint32_t> ids(count);
            std::vector<const void *> datas(count);
            std::vector<uint32_t> sizes(count);
            result = run(ids.data(), datas.data(), sizes.data());
        }

        if (invalidCount > 0) {
            ONDEMANDLOG_TIME(warning, 5000) << "setVarDataBatch: " << invalidCount << " / " << count
                                            << " items have invalid varId or null data";
        }
        if (clippedSizeCount > 0) {
            ONDEMANDLOG_TIME(warning, 5000) << "setVarDataBatch: " << clippedSizeCount
                                            << " items size overflowed uint32_t and were clipped";
        }
        return result;
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
        if (nodeSlotMap_.size() >= 64) {
            ONDEMANDLOG(error) << "Node slot overflow! Max 64 subscriber nodes supported."
                               << " nodeHash=" << nodeHash;
            return 0xFF; // kInvalidNodeSlot
        }
        /* 从 nodeSlotMap_ 已占用的 slot 中找第一个空位 */
        uint64_t used = 0;
        for (const auto &[_, s] : nodeSlotMap_) {
            used |= (1ULL << s);
        }
        uint8_t slot = static_cast<uint8_t>(__builtin_ctzll(~used));
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
        const std::string ownPrefix = nodeName_ + "_";

        // 收集需要处理的变量信息
        struct VarEntry {
            uint64_t varHash;
            VarMetadata *meta;
            uint32_t freq;
        };
        std::vector<VarEntry> entries;
        std::vector<std::pair<std::string, uint32_t>> freqChanges;
        uint32_t missingCount = 0;

        std::unique_lock lock(varIndexMutex_);

        /*订阅者数量限制*/
        bool isNewNode = (nodeSlotMap_.find(nodeHash) == nodeSlotMap_.end());
        if (isNewNode && nodeSlotMap_.size() >= maxNodeNum_) {
            ONDEMANDLOG(warning) << "Rejecting subscribe from node=" << nodeName
                                 << ": reached max subscriber limit (" << maxNodeNum_ << ")";
            return 0;
        }

        uint8_t nodeBit = getOrAssignNodeBit(nodeHash);
        if (nodeBit == 0xFF) {
            ONDEMANDLOG(error) << "Rejecting subscribe from node=" << nodeName
                               << ": exceeded max 64 subscriber nodes";
            return 0;
        }
        uint64_t nodeMask = uint64_t(1) << nodeBit;

        /*每节点变量数限制：计算本次请求新增的变量数*/
        uint32_t currentVarCount = nodeVarCount_[nodeHash];
        uint32_t newVarsInRequest = 0;
        for (const auto &varFreq : varFreqs) {
            std::string metaName = varFreq.name();
            if (metaName.compare(0, ownPrefix.size(), ownPrefix) != 0) {
                continue;
            }
            uint64_t varHash = fast_hash(metaName);
            auto it = varIndex_.find(varHash);
            if (it != varIndex_.end()) {
                bool alreadySubscribed = false;
                for (const auto &fs : it->second.freqSubs) {
                    if (fs.subMask & nodeMask) {
                        alreadySubscribed = true;
                        break;
                    }
                }
                if (!alreadySubscribed) {
                    ++newVarsInRequest;
                }
            } else {
                ++newVarsInRequest;
            }
        }
        if (maxVarsPerNode_ > 0 && currentVarCount + newVarsInRequest > maxVarsPerNode_) {
            ONDEMANDLOG(warning) << "Rejecting subscribe from node=" << nodeName
                                 << ": would exceed max vars per node (" << maxVarsPerNode_
                                 << "), current=" << currentVarCount
                                 << " requested=" << newVarsInRequest;
            return 0;
        }

        // ── 第一阶段：收集有效变量 + 批量扩展 ──
        // 支持懒创建：第一轮收集 pendingCreates → createVars → 第二轮重新收集
        std::vector<uint32_t> expandIds;
        std::vector<uint32_t> expandSizes;
        expandIds.reserve(varFreqs.size());
        expandSizes.reserve(varFreqs.size());

        for (int pass = 0; pass < 2; ++pass) {
            std::vector<DSF::Var::Define> pendingCreates;

            for (const auto &varFreq : varFreqs) {
                std::string metaName = varFreq.name();

                if (metaName.compare(0, ownPrefix.size(), ownPrefix) != 0) {
                    continue;
                }

                uint64_t varHash = fast_hash(metaName);
                auto it = varIndex_.find(varHash);
                if (it == varIndex_.end()) {
                    // 第一轮：查 liteBucketMembers_，找到则加入待创建列表
                    if (pass == 0) {
                        uint32_t bucketIdx = static_cast<uint32_t>(
                            BucketManager::CalculateBucketIndexFromHash(varHash));
                        std::shared_lock liteLock(liteVarIndexMutex_);
                        const auto &bucket = liteBucketMembers_[bucketIdx];
                        if (bucket.hashSet.count(varHash)) {
                            const LiteVarEntry *found = nullptr;
                            for (const auto &e : bucket.entries) {
                                if (e.hash == varHash) { found = &e; break; }
                            }
                            if (found) {
                                DSF::Var::Define def;
                                def.name(namePool_.data() + found->nameOffset);
                                def.nodeName(nodeName_);
                                pendingCreates.push_back(std::move(def));
                            }
                        } else {
                            ++missingCount;
                        }
                    } else {
                        // 第二轮：createVars 后仍然找不到，真的不存在
                        ++missingCount;
                    }
                    continue;
                }

                auto &meta = it->second;

                // 收集需要扩展的变量
                auto idxIt = defineLookup_.find(varHash);
                if (idxIt != defineLookup_.end()) {
                    uint32_t defineSize =
                        static_cast<uint32_t>(defineCache_[idxIt->second].size());
                    uint32_t targetSize = defineSize > 0 ? defineSize : 32u;
                    if (varStore_.slot_size(meta.varId) < targetSize) {
                        expandIds.push_back(meta.varId);
                        expandSizes.push_back(targetSize);
                    }
                }

                // 解析频率
                uint32_t freq;
                auto result = std::from_chars(varFreq.value().data(),
                                              varFreq.value().data() + varFreq.value().size(), freq);
                if (std::errc() != result.ec) {
                    ONDEMANDLOG(warning) << "Invalid frequency value for var: " << metaName
                                         << " node: " << nodeName
                                         << " value: " << varFreq.value();
                    continue;
                }

                entries.push_back({varHash, &meta, freq});
            }

            // 第一轮：有待创建变量 → createVars → 重跑第二轮
            if (pass == 0 && !pendingCreates.empty()) {
                ONDEMANDLOG(info) << "handleSubscribe: lazy creating " << pendingCreates.size()
                                  << " vars for node=" << nodeName;
                lock.unlock();
                createVars(pendingCreates);
                lock.lock();
                entries.clear();
                expandIds.clear();
                expandSizes.clear();
                missingCount = 0;
                continue; // 第二轮
            }
            break; // 无需懒创建或已完成第二轮
        }

        // 批量扩展（一次 arena 重分配）
        if (!expandIds.empty()) {
            varStore_.ensure_capacity_batch(expandIds.data(), expandSizes.data(), expandIds.size());
        }

        // ── 第二阶段：处理订阅逻辑 ──
        for (const auto &entry : entries) {
            auto &meta = *entry.meta;
            uint32_t freq = entry.freq;

            // 先从该节点已有的其他频率条目中移除
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

            // 在 freqSubs 中找到匹配 freq 的条目，或新建
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

            target->subMask |= nodeMask;
            target->subCount++;
            meta.activeFreqCount = static_cast<uint8_t>(meta.freqSubs.size());

            uint32_t oldFreq = meta.currentFreq;
            recalcCurrentFreq(meta);
            schedulerDirty_.store(true, std::memory_order_release);

            if (meta.currentFreq != oldFreq) {
                freqChanges.emplace_back(meta.realVarName, meta.currentFreq);
            }

            ONDEMANDLOG(info) << "Var [" << meta.realVarName << "] subscribed by node [" << nodeName
                              << "] at freq=" << freq << "ms, currentFreq=" << meta.currentFreq
                              << "ms";
        }

        /*更新该节点的已订阅变量计数*/
        nodeVarCount_[nodeHash] += static_cast<uint32_t>(entries.size());
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

        /*更新该节点的已订阅变量计数*/
        auto countIt = nodeVarCount_.find(nodeHash);
        if (countIt != nodeVarCount_.end()) {
            uint32_t unsubscribed = static_cast<uint32_t>(varFreqs.size());
            countIt->second = (countIt->second > unsubscribed) ? countIt->second - unsubscribed : 0;
        }

        lock.unlock();
        
        for (const auto &[varName, newFreq] : freqChanges) {
            freqChangeQueue_.enqueue({varName, newFreq});
        }
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
            defineCache_.clear();
            defineLookup_.clear();
            bucketManager_.Clear();
            nodeSlotMap_.clear();
        }

        /*重置 varStore_，清空 table/metas/arena，避免重新注册时 id 冲突*/
        varStore_.reset();

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
        {
            std::lock_guard<std::mutex> lk(pubTableDefRetryQueueMutex_);
            while (!pubTableDefRetryQueue_.empty()) {
                pubTableDefRetryQueue_.pop();
            }
        }

        size_t drainedFreqChange = 0;
        std::pair<std::string, uint32_t> freqChangeItem;
        while (freqChangeQueue_.try_dequeue(freqChangeItem)) {
        }

        ONDEMANDLOG(info) << "OnDemandPub stopped";
    }

} // namespace ondemand
} // namespace dsf
