/**
 * @file on_demand_sub.cc
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

#include <functional>
#include <mutex>
#include "on_demand_sub.h"
#include "concurrentqueue.h"
#include "ondemand/on_demand_common.h"
#include "roaring/roaring64map.hh"
#include <cstring>
#include <shared_mutex>
#include <chrono>

namespace dsf
{
namespace ondemand
{

    OnDemandSub::OnDemandSub()
        : nodeName_(), dataNode_(nullptr), pubTableDefineReader_(nullptr),
          subTableRegisterReqWriter_(nullptr), initialized_(false), running_(false),
          totalReceived_(0), varWriteStamps_{}
    {
    }

    OnDemandSub::~OnDemandSub() { stop(); }

    /**
     * @brief 初始化订阅器
     * @param  nodeName 节点名称
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::init(const std::string &nodeName)
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

        /*创建变量定义接收reader*/
        if (!createTableDefineReader(std::bind(&OnDemandSub::onReceiveTableDefineCb, this,
                                               std::placeholders::_1, std::placeholders::_2))) {
            ONDEMANDLOG(error) << "Failed to create TableDefine reader";
            initialized_.store(false);
            return false;
        }

        /*定义变量注册writer*/
        if (!createSubTableRegisterWriter()) {
            ONDEMANDLOG(error) << "Failed to create SubTableRegister writer";
            initialized_.store(false);
            return false;
        }

        /*创建数据通信所需的subscriber*/
        DdsWrapper::SubscriberQoSBuilder subQos;
        subQos.setAutoEnable(true);
        if (!dataNode_->createSubscriber(DATA_TANSFER_PUB_SUB_NAME, subQos)) {
            ONDEMANDLOG(error) << "Failed to create subscriber: " << DATA_TANSFER_PUB_SUB_NAME;
            initialized_.store(false);
            return false;
        }

        ONDEMANDLOG(info) << "OnDemandSub initialized: " << nodeName;
        /*确保 DDS endpoints 就绪: assertLiveliness 强制发送 PDP 心跳*/
        dataNode_->assertLiveliness();
        return true;
    }

    /**
     * @brief 创建变量定义数据读取器
     * @param  processFunc 数据处理回调函数
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::createTableDefineReader(
        std::function<void(const std::string &, std::shared_ptr<DSF::Var::PubTableDefine>)>
            processFunc)
    {

        constexpr uint32_t depth = 60;
        DdsWrapper::DataReaderQoSBuilder readerQosBuilder;
        readerQosBuilder.setMaxSamples(32 * depth)
            .setMaxInstances(32)
            .setMaxSamplesPerInstance(depth)
            .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
            .setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
            .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
            .setHistoryDepth(depth);

        if (0
            != dsf::ondemand::registerNodeTopicReader<DSF::Var::PubTableDefine,
                                                      DSF::Var::PubTableDefinePubSubType>(
                dataNode_, pubTableDefineReader_, DSF::Var::TABLE_DEFINE_TOPIC_NAME, processFunc,
                readerQosBuilder)) {
            ONDEMANDLOG(error)
                << "Failed to register topic for SubTableRegister: "
                << DSF::Message::MESSAGE_COMMAND_REQUEST_SUB_TABLE_REGISTER_TOPIC_NAME;

            return false;
        }

        return true;
    }

    /**
     * @brief 创建频率请求数据写入器
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::createSubTableRegisterWriter()
    {
        constexpr uint32_t depth = 100;
        DdsWrapper::DataWriterQoSBuilder writerQosBuilder;
        writerQosBuilder.setMaxSamples(256 * depth)
            .setMaxInstances(256)
            .setMaxSamplesPerInstance(depth);
        writerQosBuilder.setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
            .setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
            .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
            .setHistoryDepth(depth);

        if (0
            != dsf::ondemand::registerNodeTopicWriter<DSF::Message::SubTableRegister,
                                                      DSF::Message::SubTableRegisterPubSubType>(
                dataNode_, subTableRegisterReqWriter_,
                DSF::Message::MESSAGE_COMMAND_REQUEST_SUB_TABLE_REGISTER_TOPIC_NAME,
                writerQosBuilder)) {
            ONDEMANDLOG(error)
                << "Failed to register topic for SubTableRegister: "
                << DSF::Message::MESSAGE_COMMAND_REQUEST_SUB_TABLE_REGISTER_TOPIC_NAME;
            return false;
        }
        return true;
    }

    /**
     * @brief 创建数据传输读取器
     * @param  processFunc 数据处理回调函数
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::createDataTransferReader(
        std::function<void(const std::string &, std::shared_ptr<DSF::Var::TableDataTransfer>)>
            processFunc,
        const std::unordered_set<uint32_t> *targetBucketIds)
    {
        /*默认收集全部 bucket；若调用方提供增量 bucket，则只创建新增部分*/
        std::unordered_set<uint32_t> bucketIds;
        if (targetBucketIds != nullptr) {
            bucketIds = *targetBucketIds;
        } else {
            std::shared_lock lock(varIndexMutex_);
            for (const auto &[hash, meta] : varIndex_) {
                bucketIds.insert(static_cast<uint32_t>(meta.bucketIndex));
            }
        }

        if (bucketIds.empty()) {
            ONDEMANDLOG(warning) << "No variables registered, no data transfer readers to create.";
            return false;
        }

        constexpr uint32_t depth = 10;
        DdsWrapper::DataReaderQoSBuilder readerQosBuilder;
        // readerQosBuilder.setMaxSamples(256 * depth)
        //     .setMaxInstances(256)
        //     .setMaxSamplesPerInstance(depth)
        //     .setDurabilityKind(DdsWrapper::DurabilityKind::VOLATILE)
        //     .setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        //     .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        //     .setHistoryDepth(depth);

        std::lock_guard<std::mutex> lock(dataTransferCtxMapMutex_);
        for (uint32_t bucketId : bucketIds) {
            // 如果该 bucket 的 reader 已存在，跳过
            if (dataTransferReaderMap_.find(bucketId) != dataTransferReaderMap_.end()) {
                ONDEMANDLOG(debug)
                    << "DataTransfer reader already exists for bucketId: " << bucketId;
                continue;
            }

            std::string tableName = make_bucket_name_by_id(bucketId);
            std::string topicName = DSF::Var::VAR_DATA_TRANSFER_TOPIC_PREFIX + tableName;
            std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Var::TableDataTransfer>> reader;

            reader = dataNode_->createDataReader<DSF::Var::TableDataTransfer,
                                                 DSF::Var::TableDataTransferPubSubType>(
                topicName, DATA_TANSFER_PUB_SUB_NAME, processFunc, readerQosBuilder);

            dataTransferReaderMap_.emplace(bucketId, reader);
            ONDEMANDLOG(info) << "Created DataTransfer reader for bucketId: " << bucketId
                              << ", topic: " << topicName;
        }

        ONDEMANDLOG(info) << "Created " << dataTransferReaderMap_.size()
                          << " DataTransfer readers in total.";
        return true;
    }

    /**
     * @brief 处理变量定义数据回调函数
     * @param  topicName 主题名称
     * @param  data 变量定义数据
     */
    void OnDemandSub::onReceiveTableDefineCb(const std::string &topicName,
                                             std::shared_ptr<DSF::Var::PubTableDefine> data)
    {
        pubTableDefineQueue_.enqueue(data);
    }

    /**
     * @brief 处理接收数据传输回调函数
     * @param  topicName 主题名称
     * @param  data 数据传输数据
     */
    void OnDemandSub::onReceiveDataTransferCb(const std::string &topicName,
                                              std::shared_ptr<DSF::Var::TableDataTransfer> data)
    {
        if (dataTransferQueue_.size_approx() > 100) {
            std::shared_ptr<DSF::Var::TableDataTransfer> dropped;
            if (dataTransferQueue_.try_dequeue(dropped)) {
                ONDEMANDLOG_TIME(warning, 1000)
                    << "DataTransfer queue is full (" << dataTransferQueue_.size_approx()
                    << "), dropped oldest message to keep latest";
            } else {
                ONDEMANDLOG_TIME(warning, 1000)
                    << "DataTransfer queue is full (" << dataTransferQueue_.size_approx()
                    << "), failed to dequeue oldest, dropping latest";
                return;
            }
        }
        dataTransferQueue_.enqueue(data);
    }

    /**
     * @brief 处理接收到的数据传输消息
     *
     * mask 编码: Roaring64Map 序列化字节流
     *   - 反序列化后按升序迭代, 第 i 个 varHash 对应 varData[i]
     *   - 通过 varHash 查找本地 varId, 写入 varStore_
     */
    void OnDemandSub::processDataTransfer()
    {
        pthread_setname_np(pthread_self(), "proc_data_tx");

        while (running_.load(std::memory_order_acquire)) {
            std::shared_ptr<DSF::Var::TableDataTransfer> dataTransfer;
            if (!dataTransferQueue_.try_dequeue(dataTransfer)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (!dataTransfer) {
                continue;
            }
            const auto &maskBytes = dataTransfer->mask();
            const auto &varDataList = dataTransfer->varData();
            const auto &timeStamp = dataTransfer->timestamp();
            const auto blobType = dataTransfer->blobType();

            if (maskBytes.empty() || varDataList.empty()) {
                ONDEMANDLOG(warning) << "Received empty mask or varData, skipping.";
                continue;
            }

            /*1. 反序列化 Roaring64Map*/
            roaring::Roaring64Map roar;
            try {
                roar =
                    roaring::Roaring64Map::read(reinterpret_cast<const char *>(maskBytes.data()));
            } catch (const std::exception &e) {
                ONDEMANDLOG(error) << "Failed to deserialize mask: " << e.what();
                continue;
            }

            // 2. 迭代 Roaring64Map (升序), varData[i] 与第 i 个 hash 一一对应
            size_t idx = 0;
            size_t written = 0;
            size_t bucketIdx = SIZE_MAX;
            {
                std::shared_lock lock(varIndexMutex_);
                for (auto it = roar.begin(); it != roar.end() && idx < varDataList.size();
                     ++it, ++idx) {
                    uint64_t varHash = *it;
                    const auto &blob = varDataList[idx];
                    if (blob.empty()) {
                        ONDEMANDLOG_TIME(warning, 3000)
                            << "Received empty data blob for varHash: " << varHash << ", skipping.";
                        continue;
                    }

                    auto vit = varIndex_.find(varHash);
                    if (vit == varIndex_.end()) {
                        ONDEMANDLOG(error)
                            << "Received data for unknown varHash: " << varHash << ", skipping.";
                        continue;
                    }

                    uint32_t varId = vit->second.varId;
                    if (varId == VarStore::kInvalidId)
                        continue;

                    if (bucketIdx == SIZE_MAX) {
                        bucketIdx = vit->second.bucketIndex;
                    } else if (bucketIdx != vit->second.bucketIndex) {
                        ONDEMANDLOG_TIME(warning, 2000)
                            << "Mixed bucket data in one TableDataTransfer message, expected="
                            << bucketIdx << ", got=" << vit->second.bucketIndex
                            << ", varHash=" << varHash << ", skipping this entry";
                        continue;
                    }

                    if (varStore_.write(varId, blob.data(), blob.size())) {
                        ++written;
                    } else {
                        ONDEMANDLOG_TIME(error, 5)
                            << "Failed to write varId: " << varId << " for varHash: " << varHash;
                    }
                }
            }
            /*整张表写完后统一更新 bucket stamp，避免定时器读到半张表*/
            if (written > 0 && bucketIdx < ONDEMAND_BUCKET_SIZE) {
                uint64_t pubTsNs = static_cast<uint64_t>(timeStamp.tv_sec()) * 1000000000ULL
                                   + static_cast<uint64_t>(timeStamp.tv_nsec());
                varWriteStamps_[bucketIdx].timestampNs.store(pubTsNs, std::memory_order_release);
                varWriteStamps_[bucketIdx].blobType.store(static_cast<uint32_t>(blobType),
                                                          std::memory_order_release);
                varWriteStamps_[bucketIdx].writeCount.fetch_add(1, std::memory_order_release);
                blobType_.store(static_cast<uint32_t>(blobType), std::memory_order_release);
            }

            
        }
    }

    /**
     * @brief 处理变量定义
     */
    void OnDemandSub::processTableDefine()
    {
        pthread_setname_np(pthread_self(), "proc_tab_def");
        while (running_.load(std::memory_order_acquire)) {
            std::shared_ptr<DSF::Var::PubTableDefine> tableDefine;
            if (pubTableDefineQueue_.try_dequeue(tableDefine)) {
                if (tableDefine) {
                    ONDEMANDLOG(info) << "Processing TableDefine: " << tableDefine->name()
                                      << ", vars size: " << tableDefine->varDefines().size();
                    std::unique_lock lock(varIndexMutex_);

                    /*解析本次 TableDefine 所属 bucket（格式: bucket_N）*/
                    uint32_t thisBucketId = UINT32_MAX;
                    const std::string &tblName = tableDefine->name();
                    const std::string prefix = "bucket_";
                    if (tblName.size() > prefix.size() &&
                        tblName.compare(0, prefix.size(), prefix) == 0) {
                        try {
                            thisBucketId = static_cast<uint32_t>(
                                std::stoul(tblName.substr(prefix.size())));
                        } catch (...) {
                        }
                    }

                    /*收集本次 TableDefine 中所有变量的 hash，用于差分删除*/
                    std::unordered_set<uint64_t> incomingHashes;
                    incomingHashes.reserve(tableDefine->varDefines().size());
                    for (const auto &varDef : tableDefine->varDefines()) {
                        const auto &varDefine = varDef.var().varDefine();
                        std::string varName =
                            make_meta_varname(varDefine.nodeName(), varDefine.name());
                        incomingHashes.insert(fast_hash(varName));
                    }

                    /*差分删除：varIndex_ 中属于该 bucket 但本次 TableDefine 里没有的变量*/
                    if (thisBucketId != UINT32_MAX) {
                        std::vector<uint64_t> toRemove;
                        for (auto it = varIndex_.begin(); it != varIndex_.end();) {
                            if (it->second.bucketIndex == thisBucketId &&
                                incomingHashes.find(it->first) == incomingHashes.end()) {
                                ONDEMANDLOG(info)
                                    << "Removing deleted var: " << it->second.realVarName;
                                toRemove.push_back(it->first);
                                varStore_.unregister_var(it->first);
                                totalReceived_.fetch_sub(1);
                                it = varIndex_.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        /*同步清理对应的回调注册，避免调度器用已删除 varId 读 VarStore*/
                        if (!toRemove.empty()) {
                            std::lock_guard<std::mutex> cbLock(subscriptionCallbacksMutex_);
                            for (uint64_t h : toRemove) {
                                subscriptionCallbacks_.erase(h);
                            }
                        }
                    }

                    /*拆表：注册新增变量*/
                    std::unordered_set<uint32_t> newBucketIds;
                    for (auto &varDef : tableDefine->varDefines()) {
                        const auto &varDefine = varDef.var().varDefine();
                        std::string varName =
                            make_meta_varname(varDefine.nodeName(),
                                              varDefine.name()); // 构造全名 和发布端一致
                        uint64_t varHash = fast_hash(varName);
                        size_t bucketIdx =
                            BucketManager::CalculateBucketIndexFromHash(varHash); // Reuse hash

                        /*先检查是否已存在，避免重复 register_var*/
                        auto it = varIndex_.find(varHash);
                        if (it != varIndex_.end()) {
                            ONDEMANDLOG(warning) << "Variable already exists: " << varName;
                            continue;
                        }

                        /*组内部结构*/
                        VarMetadata meta;
                        meta.varHash = varHash;
                        meta.currentFreq = 0xFFFFFFFF;
                        meta.activeFreqCount = 0;
                        uint32_t kVarSize = varDefine.size();
                        meta.bucketIndex = bucketIdx;
                        meta.varDefine = std::make_shared<DSF::Var::Define>(varDefine);
                        meta.realVarName = varDefine.name();
                        meta.varId = varStore_.register_var(varHash, kVarSize);

                        varIndex_.emplace(varHash, std::move(meta));
                        newBucketIds.insert(static_cast<uint32_t>(bucketIdx));
                        totalReceived_.fetch_add(1);
                        ONDEMANDLOG(debug) << "Registered var: " << varName;
                    }
                    lock.unlock();

                    /*通知外层同步 define 到 Var::state*/
                    {
                        std::lock_guard<std::mutex> cbLock(tableDefineCbMutex_);
                        if (tableDefineCb_) {
                            std::vector<DSF::Var::Define> defines;
                            defines.reserve(tableDefine->varDefines().size());
                            for (const auto &varDef : tableDefine->varDefines()) {
                                defines.push_back(varDef.var().varDefine());
                            }
                            tableDefineCb_(defines);
                        }
                    }

                    /*初始化内存: register_var 只写元数据，必须 finalize 才分配 arena/dirty_flags*/
                    if (!varStore_.finalize()) {
                        ONDEMANDLOG(error) << "Failed to finalize VarStore after TableDefine";
                    }

                    /* 这里很重要哦！！！！ varIndex_ 已更新，通知回调调度器重新扫描建立定时器
                     * 解决 sub 先启动时 callbackDirty_ 已被消费但定时器未建立的问题 */
                    callbackDirty_.store(true, std::memory_order_release);

                    /*仅按本次新增 bucket 增量创建 reader，避免每次全量扫描 bucket*/
                    if (!newBucketIds.empty()) {
                        createDataTransferReader(std::bind(&OnDemandSub::onReceiveDataTransferCb,
                                                           this, std::placeholders::_1,
                                                           std::placeholders::_2),
                                                 &newBucketIds);
                    }
                }
                LOG(info) << "+++++++++++++++++totalReceived_ = " << totalReceived_.load();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    /**
     * @brief 启动订阅器
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::start()
    {
        if (!initialized_.load(std::memory_order_acquire)) {
            ONDEMANDLOG(error) << "OnDemandSub not initialized";
            return false;
        }
        if (running_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already running";
            return false;
        }

        processTableDefineThread_ = std::thread(&OnDemandSub::processTableDefine, this);
        {
            size_t workerCount = 4;
            processDataTransferThreads_.clear();
            processDataTransferThreads_.reserve(workerCount);
            for (size_t i = 0; i < workerCount; ++i) {
                processDataTransferThreads_.emplace_back(&OnDemandSub::processDataTransfer, this);
            }
            ONDEMANDLOG(info) << "Started data transfer workers: " << workerCount;
        }

        /*启动回调调度器 (tick 精度 1ms, 线程池 4 线程)*/

        callbackScheduler_ = std::make_unique<TimerScheduler>(1, 4);
        /*调度函数*/
        callbackSchedulerThread_ = std::thread(&OnDemandSub::processCallbackScheduler, this);

        ONDEMANDLOG(info) << "OnDemandSub started";
        return true;
    }

    bool OnDemandSub::setPartition(std::string name, std::string partitionName)
    {
        DdsWrapper::SubscriberQoSBuilder subQos;
        subQos.setAutoEnable(true);
        subQos.setPartition(partitionName);
        if (!dataNode_->updateSubscriberQos(name, subQos)) {
            ONDEMANDLOG(error) << "Failed to set partition: " << partitionName
                               << " for subscriber: " << name;
            return false;
        }
        return true;
    }

    /**
     * @brief 订阅变量并注册回调
     * @param  node_name 节点名
     * @param  items 变量信息列表
     * @param  callback 数据回调函数，在时间轮定时器触发时被调用
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::subscribe(const char *node_name, const std::vector<SubscriptionItem> &items,
                                DataCallback callback)
    {
        if (!initialized_.load(std::memory_order_acquire)) {
            ONDEMANDLOG(error) << "OnDemandSub not initialized";
            return false;
        }
        std::string tableName;
        DSF::Message::SubTableRegister subReq;
        {
            std::shared_lock lock(varIndexMutex_);

            for (const auto &item : items) {
                /*1.计算点hash*/
                std::string metaVarName = make_meta_varname(node_name, item.varName);
                uint64_t varHash = fast_hash(metaVarName);
                ONDEMANDLOG(debug)
                    << "Subscribing to var: " << item.varName << " with hash: " << varHash;
                auto it = varIndex_.find(varHash);
                tableName = make_bucket_name_by_hash(varHash);
                if (it == varIndex_.end()) {
                    // ONDEMANDLOG(warning) << "Variable not found for subscription: " << item.varName;
                    // continue;  这里考虑到有可能订阅请求先于变量定义到达，所以不直接跳过
                }

                /*组包*/
                DSF::NamedValue varFreq;
                varFreq.name(metaVarName);
                varFreq.value(std::to_string(item.frequency));
                subReq.varFreqs().emplace_back(varFreq);
            }
        }

        /*开始组包*/
        subReq.msgType(DSF::Message::MSGTYPE::SUB_TABLE_REGISTER);
        subReq.nodeName(nodeName_);
        subReq.tableName(tableName);
        if (subReq.varFreqs().empty()) {
            ONDEMANDLOG(warning) << "SubTableRegister has no variables for table: " << tableName;
            return false;
        }

        /*发布注册信息*/
        auto writer = subTableRegisterReqWriter_;
        if (!writer || !writer->writeMessage(subReq)) {
            ONDEMANDLOG(error) << "Failed to publish SubTableRegister for table: " << tableName;
            return false;
        }

        /*存储回调信息到本地, 供时间轮调度器使用*/
        if (callback) {
            std::lock_guard<std::mutex> lock(subscriptionCallbacksMutex_);
            size_t registered = 0;
            for (const auto &item : items) {
                if (item.frequency == 0 || item.frequency == 0xFFFFFFFF) {
                    ONDEMANDLOG(warning)
                        << "Invalid frequency (" << item.frequency << ") for var: " << item.varName
                        << ", skipping callback registration";
                    continue;
                }
                std::string metaVarName = make_meta_varname(node_name, item.varName);
                uint64_t varHash = fast_hash(metaVarName);
                SubCallbackInfo info;
                info.freqMs = item.frequency;
                info.callback = callback;
                info.varName = item.varName;
                subscriptionCallbacks_[varHash] = std::move(info);
                ++registered;
            }
            callbackDirty_.store(true, std::memory_order_release);
            ONDEMANDLOG(info) << "Registered " << registered
                              << " callback subscriptions for node: " << node_name;
        }

        /*打开相应的通道*/
        setPartition(DATA_TANSFER_PUB_SUB_NAME,
                     DSF::Var::VAR_DATA_TRANSFER_TOPIC_PREFIX + node_name);
        return true;
    }

    /**
     * @brief 取消订阅
     * @param  node_name 节点名
     * @param  items 变量名列表
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::unsubscribe(const char *node_name, const std::vector<std::string> &items)
    {
        if (!initialized_.load(std::memory_order_acquire)) {
            ONDEMANDLOG(error) << "OnDemandSub not initialized";
            return false;
        }
        std::string tableName;
        DSF::Message::SubTableRegister subReq;
        {
            std::shared_lock lock(varIndexMutex_);

            for (const auto &item : items) {
                DSF::NamedValue varFreq;
                /*计算点hash*/
                std::string metaVarName = make_meta_varname(node_name, item);
                uint64_t varHash = fast_hash(metaVarName);
                ONDEMANDLOG(debug)
                    << "Unsubscribing from var: " << item << " with hash: " << varHash;
                auto it = varIndex_.find(varHash);
                tableName = make_bucket_name_by_hash(varHash);
                if (it == varIndex_.end()) {
                    ONDEMANDLOG(warning) << "Variable not found for subscription: " << item;
                    // continue;  这里考虑到有可能订阅请求先于变量定义到达，所以不直接跳过
                }
                varFreq.name(metaVarName);
                varFreq.value();
                subReq.varFreqs().emplace_back(varFreq);
            }
        }

        /*开始组包*/
        subReq.msgType(DSF::Message::MSGTYPE::SUB_TABLE_UNREGISTER);
        subReq.nodeName(nodeName_);
        subReq.tableName(tableName);
        if (subReq.varFreqs().empty()) {
            ONDEMANDLOG(warning) << "SubTableRegister has no variables for table: " << tableName;
            return false;
        }

        /*发布注销信息*/
        auto writer = subTableRegisterReqWriter_;
        if (!writer || !writer->writeMessage(subReq)) {
            ONDEMANDLOG(error) << "Failed to publish SubTableRegister for table: " << tableName;
            return false;
        }

        /*移除本地回调信息*/
        {
            std::lock_guard<std::mutex> lock(subscriptionCallbacksMutex_);
            for (const auto &item : items) {
                std::string metaVarName = make_meta_varname(node_name, item);
                uint64_t varHash = fast_hash(metaVarName);
                subscriptionCallbacks_.erase(varHash);
            }
            callbackDirty_.store(true, std::memory_order_release);
            ONDEMANDLOG(info) << "Removed " << items.size()
                              << " callback subscriptions for node: " << node_name;
        }

        return true;
    }

    /**
     * @brief 回调调度器主循环，扫描订阅回调信息并管理时间轮定时器
     */
    void OnDemandSub::processCallbackScheduler()
    {
        pthread_setname_np(pthread_self(), "SubCbScheduler");
        constexpr auto kScanInterval = std::chrono::milliseconds(50);

        while (running_.load(std::memory_order_acquire)) {

            std::this_thread::sleep_for(kScanInterval);

            /* 只在订阅信息有变更时才重建分组 */
            if (!callbackDirty_.exchange(false, std::memory_order_acq_rel)) {
                continue;
            }

            /* 从 subscriptionCallbacks_ + varIndex_ 构建期望分组 */
            using DesiredMap =
                std::unordered_map<CallbackGroupKey, std::shared_ptr<std::vector<CallbackVarInfo>>,
                                   CallbackGroupKeyHash>;

            DesiredMap desired;
            {
                std::lock_guard<std::mutex> cbLock(subscriptionCallbacksMutex_);
                std::shared_lock varLock(varIndexMutex_);
                for (const auto &[varHash, cbInfo] : subscriptionCallbacks_) {
                    /* 查找 varIndex_ 获取 varId 和 dataSize */
                    auto vit = varIndex_.find(varHash);
                    if (vit == varIndex_.end()) {
                        /* 变量定义尚未到达, 或者删除时 跳过 */
                        continue;
                    }

                    int32_t varId = vit->second.varId;
                    if (varId < 0 || static_cast<uint32_t>(varId) == VarStore::kInvalidId) {
                        /* varStore 尚未 finalize 或注册失败，跳过 */
                        continue;
                    }

                    CallbackGroupKey key{static_cast<uint32_t>(vit->second.bucketIndex),
                                         cbInfo.freqMs};
                    auto &vec = desired[key];
                    if (!vec) {
                        vec = std::make_shared<std::vector<CallbackVarInfo>>();
                    }
                    CallbackVarInfo vi;
                    vi.varHash = varHash;
                    vi.varId = varId;
                    vi.dataSize = varStore_.slot_size(static_cast<uint32_t>(varId));
                    vi.bucketIndex = static_cast<uint32_t>(vit->second.bucketIndex);
                    if (vit->second.varDefine) {
                        vi.nodeName = vit->second.varDefine->nodeName();
                        vi.varType = vit->second.varDefine->modelName();
                        vi.typeVersion = vit->second.varDefine->modelVersion();
                    } else {
                        ONDEMANDLOG(warning) << "varDefine is null for varHash=" << varHash
                                             << ", callback will receive empty nodeName/varType";
                    }
                    vi.varName = cbInfo.varName;
                    vi.callback = cbInfo.callback;
                    vec->push_back(std::move(vi));
                }
            }

            /* 增量*/
            {
                std::lock_guard<std::mutex> lock(callbackGroupsMutex_);

                /* 移除不再需要的分组， 在目前运行的时间轮管理器找一下该频率，如果此次的频率不在该时间轮，就干掉*/
                for (auto it = callbackGroupTimers_.begin(); it != callbackGroupTimers_.end();) {
                    if (desired.find(it->first) == desired.end()) {
                        if (callbackScheduler_) {
                            callbackScheduler_->Cancel(it->second);
                        }
                        callbackGroupMembers_.erase(it->first);
                        ONDEMANDLOG(debug)
                            << "Removed callback group: freq=" << it->first.freqMs << "ms";
                        it = callbackGroupTimers_.erase(it);
                    } else {
                        ++it;
                    }
                }

                /* 新增或更新分组 */
                for (auto &[key, members] : desired) {
                    /* 始终刷新成员列表，保留已有的 running flag */
                    callbackGroupMembers_[key].members = std::move(members);

                    /* 仅为新增分组创建定时器，已有分组的定时器保持不变 */
                    if (callbackGroupTimers_.find(key) == callbackGroupTimers_.end()) {
                        /* 确保新分组有 running flag */
                        if (!callbackGroupMembers_[key].running) {
                            callbackGroupMembers_[key].running =
                                std::make_shared<std::atomic<bool>>(false);
                        }
                        uint32_t bucketIdx = key.bucketIndex;
                        uint32_t freqMs = key.freqMs;
                        Tick intervalTicks = static_cast<Tick>(freqMs);
                        /* 与 pub 保持同样错峰；最小步长 5 ticks，再加 5ms 确保 pub 已写完 */
                        Tick staggerStep = static_cast<Tick>(freqMs / ONDEMAND_BUCKET_SIZE);
                        constexpr Tick kMinStaggerTicks = 5;
                        if (staggerStep < kMinStaggerTicks) {
                            staggerStep = kMinStaggerTicks;
                        }
                        Tick jitter = static_cast<Tick>(bucketIdx) * staggerStep + 5;
                        auto timer = callbackScheduler_->ScheduleRecurring(
                            [this, bucketIdx, freqMs]() { callbackGroupData(bucketIdx, freqMs); },
                            intervalTicks + jitter, /* 首次延迟，比 pub 晚 5ms */
                            intervalTicks           /* 周期 */
                        );
                        callbackGroupTimers_[key] = timer;
                        ONDEMANDLOG(debug)
                            << "Created callback group: bucket=" << bucketIdx << " freq=" << freqMs
                            << "ms, members=" << callbackGroupMembers_[key].members->size();
                    }
                }
            }
        }
    }

    /**
     * @brief 回调分组数据: 读取 VarStore 并调用用户注册的 DataCallback
     * @param bucketIndex bucket 索引
     * @param freqMs 回调频率 (ms), 用于定位分组
     */
    void OnDemandSub::callbackGroupData(uint32_t bucketIndex, uint32_t freqMs)
    {

        /* 获取组成员快照 + running flag */
        std::shared_ptr<std::vector<CallbackVarInfo>> members;
        std::shared_ptr<std::atomic<bool>> running;
        {
            std::lock_guard<std::mutex> lock(callbackGroupsMutex_);
            CallbackGroupKey key{bucketIndex, freqMs};
            auto it = callbackGroupMembers_.find(key);
            if (it == callbackGroupMembers_.end() || !it->second.members
                || it->second.members->empty()) {
                return;
            }
            members = it->second.members;
            running = it->second.running;
        }

        /* 防止并发执行：上次还没执行完则跳过本次 */
        if (!running) {
            return;
        }
        if (running->exchange(true, std::memory_order_acq_rel)) {
            ONDEMANDLOG_TIME(debug, 2000)
                << "Skip callback group because previous invocation is still running: bucket="
                << bucketIndex << " freq=" << freqMs << "ms";
            return;
        }
        struct RunningGuard {
            std::atomic<bool> &flag;
            ~RunningGuard() { flag.store(false, std::memory_order_release); }
        } guard{*running};

        const DataCallback *groupCallback = nullptr;
        std::vector<VarCallbackData> batch;
        batch.reserve(members->size());

        uint64_t tsNs = 0;
        DSF::Var::BLOB_TYPE blobType = DSF::Var::BLOB_TYPE::UNKNOWN;
        if (bucketIndex < ONDEMAND_BUCKET_SIZE) {
            // 通过前后两次 writeCount 快照，避免读到跨写入周期的时间戳/类型组合
            bool hasStableStamp = false;
            for (int retry = 0; retry < 3; ++retry) {
                uint32_t beginCount =
                    varWriteStamps_[bucketIndex].writeCount.load(std::memory_order_acquire);
                if (beginCount == 0) {
                    return;
                }
                uint64_t snapshotTs =
                    varWriteStamps_[bucketIndex].timestampNs.load(std::memory_order_acquire);
                uint32_t snapshotBlob =
                    varWriteStamps_[bucketIndex].blobType.load(std::memory_order_acquire);
                uint32_t endCount =
                    varWriteStamps_[bucketIndex].writeCount.load(std::memory_order_acquire);
                if (beginCount == endCount) {
                    tsNs = snapshotTs;
                    blobType = static_cast<DSF::Var::BLOB_TYPE>(snapshotBlob);
                    hasStableStamp = true;
                    break;
                }
            }
            if (!hasStableStamp) {
                tsNs = varWriteStamps_[bucketIndex].timestampNs.load(std::memory_order_acquire);
                blobType = static_cast<DSF::Var::BLOB_TYPE>(
                    varWriteStamps_[bucketIndex].blobType.load(std::memory_order_acquire));
            }
        }

        /*ai修复死锁问题 逐个读取并立即释放 handle，避免同时持有多个 handle 导致与 finalize()
         * 的 config_begin() 死锁（finalize 等 active_ops==0，而多个 handle
         * 同时存活会让 active_ops 无法归零）。
         * 用 (offset, size) 记录位置，循环结束后再统一修正 batch 指针。*/
        struct CopiedItem {
            const CallbackVarInfo *info;
            size_t offset;
            uint32_t size;
        };
        std::vector<CopiedItem> copied;
        copied.reserve(members->size());
        std::vector<uint8_t> dataBuf;
        /* 用 dataSize 做预估，避免固定 *64 在大变量场景下频繁 realloc */
        size_t reserveBytes = 0;
        for (const auto &info : *members)
            reserveBytes += info.dataSize > 0 ? info.dataSize : 64u;
        dataBuf.reserve(reserveBytes);

        for (const auto &info : *members) {
            if (!info.callback)
                continue;

            size_t off = dataBuf.size();
            uint32_t sz = 0;
            {
                auto handle = varStore_.read_zero_copy(static_cast<uint32_t>(info.varId));
                if (!handle || handle.size() == 0) {
                    ONDEMANDLOG_TIME(debug, 5000) << "Skip unregistered varId: " << info.varId
                                                  << " varName: " << info.varName;
                    continue;
                }
                if (!groupCallback)
                    groupCallback = &info.callback;
                sz = handle.size();
                const auto *src = reinterpret_cast<const uint8_t *>(handle.ptr());
                dataBuf.insert(dataBuf.end(), src, src + sz);
            } // handle 析构，op_exit()，active_ops 归零，finalize 可推进
            copied.push_back({&info, off, sz});
        }

        if (copied.empty() || !groupCallback)
            return;

        /* dataBuf 不再增长，指针稳定，统一填 batch */
        for (const auto &item : copied) {
            const auto &info = *item.info;
            batch.push_back({info.nodeName, info.varName, info.varType, info.typeVersion,
                             dataBuf.data() + item.offset, item.size, tsNs, blobType});
        }

        if (batch.empty())
            return;

        try {
            (*groupCallback)(batch);
        } catch (const std::exception &e) {
            ONDEMANDLOG_TIME(error, 5000) << "Batch callback exception for freq=" << freqMs
                                          << "ms, vars=" << batch.size() << " err: " << e.what();
        } catch (...) {
            ONDEMANDLOG_TIME(error, 5000) << "Batch callback unknown exception for freq=" << freqMs
                                          << "ms, vars=" << batch.size();
        }
    }

    /**
     * @brief 取消所有回调分组定时器并清空分组成员索引
     */
    void OnDemandSub::cancelAllCallbackTimers()
    {
        std::lock_guard<std::mutex> lock(callbackGroupsMutex_);

        for (auto &[key, timer] : callbackGroupTimers_) {
            if (callbackScheduler_) {
                callbackScheduler_->Cancel(timer);
            }
        }
        callbackGroupTimers_.clear();
        callbackGroupMembers_.clear();

        ONDEMANDLOG(info) << "All callback group timers canceled";
    }

    void OnDemandSub::onWriterDiscovery(const DdsWrapper::EndpointInfo &info)
    {
        ONDEMANDLOG(debug) << "[sub node]Writer discovery: topic=" << info.topic_name
                           << " type=" << info.type_name << " discovered=" << info.discovered;
    }

    std::unordered_map<std::string, std::vector<std::string>> OnDemandSub::getAvailableVars() const
    {
        std::unordered_map<std::string, std::vector<std::string>> result;
        std::shared_lock lock(varIndexMutex_);
        for (const auto &[hash, meta] : varIndex_) {
            if (!meta.varDefine) {
                ONDEMANDLOG(critical)
                    << "Variable with hash " << hash << " has no definition, skipping.";
                continue;
            }

            result[meta.varDefine->nodeName()].push_back(meta.varDefine->name());
        }
        return result;
    }

    /**
     * @brief 停止订阅器
     */
    void OnDemandSub::stop()
    {
        initialized_.store(false);
        if (!running_.exchange(false)) {
            return;
        }

        /*先停止回调调度器（停止时间轮线程池）*/
        if (callbackScheduler_) {
            callbackScheduler_->Stop();
        }

        /*再取消所有回调定时器*/
        cancelAllCallbackTimers();

        /*等待所有线程退出*/
        if (processTableDefineThread_.joinable()) {
            processTableDefineThread_.join();
        }
        for (auto &t : processDataTransferThreads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        processDataTransferThreads_.clear();
        if (callbackSchedulerThread_.joinable()) {
            callbackSchedulerThread_.join();
        }

        /*最后清理调度器对象*/
        if (callbackScheduler_) {
            callbackScheduler_.reset();
        }

        pubTableDefineReader_.reset();
        pubTableDefineReader_ = nullptr;
        subTableRegisterReqWriter_.reset();
        subTableRegisterReqWriter_ = nullptr;

        {
            std::lock_guard<std::mutex> lock(dataTransferCtxMapMutex_);
            dataTransferReaderMap_.clear();
        }

        totalReceived_.store(0);

        std::shared_ptr<DSF::Var::PubTableDefine> dummy;
        while (pubTableDefineQueue_.try_dequeue(dummy)) {
            // Dequeue
        }
        std::shared_ptr<DSF::Var::TableDataTransfer> dummyData;
        while (dataTransferQueue_.try_dequeue(dummyData)) {
            // Dequeue
        }

        {
            std::lock_guard<std::mutex> lock(subscriptionCallbacksMutex_);
            subscriptionCallbacks_.clear();
        }

        for (auto &s : varWriteStamps_) {
            s.writeCount.store(0);
            s.timestampNs.store(0);
        }

        dataNode_.reset();
        dataNode_ = nullptr;
        ONDEMANDLOG(info) << "OnDemandSub stopped";
    }

} // namespace ondemand
} // namespace dsf
