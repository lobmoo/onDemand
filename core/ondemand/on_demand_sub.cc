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
#include <chrono>

namespace dsf
{
namespace ondemand
{

    OnDemandSub::OnDemandSub()
        : nodeName_(), dataNode_(nullptr), pubTableDefineReader_(nullptr),
          subTableRegisterReqWriter_(nullptr), initialized_(false), running_(false),
          totalReceived_(0)
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
        DdsWrapper::ParticipantQoSBuilder qos_configurator;
        qos_configurator.addUDPV4TransportInterfaces({"10.25.5.26"})
            .setDiscoveryMulticastLocator("239.255.0.1", 7400)
            .setUserMulticastLocator("239.255.0.1", 7401)
            .addFlowController()
            .setDiscoveryKeepAlive(2000, 500)
            .setInitialAnnouncements(30, 100); // 10次PDP公告, 100ms间隔, 确保3秒内完成初始发现
        try {
            dataNode_ =
                std::make_shared<DdsWrapper::DataNode>(DOMAIN_ID, nodeName, qos_configurator, this);
        } catch (const std::exception &e) {
            ONDEMANDLOG(error) << "Failed to create DataNode: " << e.what();
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

        constexpr uint32_t depth = 1;
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
            processFunc)
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
            if (0
                != dsf::ondemand::registerNodeTopicReader<DSF::Var::TableDataTransfer,
                                                          DSF::Var::TableDataTransferPubSubType>(
                    dataNode_, reader, topicName, processFunc, readerQosBuilder)) {
                ONDEMANDLOG(error)
                    << "Failed to create DataTransfer reader for topic: " << topicName;
                return false;
            }
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
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::onReceiveTableDefineCb(const std::string &topicName,
                                             std::shared_ptr<DSF::Var::PubTableDefine> data)
    {
        pubTableDefineQueue_.enqueue(data);
        return true;
    }

    /**
     * @brief 处理接收数据传输回调函数
     * @param  topicName 主题名称
     * @param  data 数据传输数据
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::onReceiveDataTransferCb(const std::string &topicName,
                                              std::shared_ptr<DSF::Var::TableDataTransfer> data)
    {
        dataTransferQueue_.enqueue(data);
        return true;
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
        pthread_setname_np(pthread_self(), "proc_data_transfer");

        while (running_.load(std::memory_order_acquire)) {
            std::shared_ptr<DSF::Var::TableDataTransfer> dataTransfer;
            if (!dataTransferQueue_.try_dequeue(dataTransfer)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
            {
                std::shared_lock lock(varIndexMutex_);
                for (auto it = roar.begin(); it != roar.end() && idx < varDataList.size();
                     ++it, ++idx) {
                    uint64_t varHash = *it;
                    const auto &blob = varDataList[idx];
                    if (blob.empty()) {
                        ONDEMANDLOG(warning)
                            << "Received empty data blob for varHash: " << varHash << ", skipping.";
                        continue;
                    }

                    /*查找本地 varId*/
                    auto vit = varIndex_.find(varHash);
                    if (vit == varIndex_.end()) {
                        ONDEMANDLOG(warning)
                            << "Received data for unknown varHash: " << varHash << ", skipping.";
                        continue; // 本节点未订阅该变量, 跳过
                    }

                    int32_t varId = vit->second.varId;
                    if (varId < 0) {
                        ONDEMANDLOG(warning)
                            << "Invalid varId for varHash: " << varHash << ", skipping.";
                        continue;
                    }

                    /*写入 varStore*/
                    if (varStore_.write(varId, blob.data(), blob.size())) {
                        /*更新写入时间戳、计数和 blobType, 供回调侧检测时效*/
                        if (static_cast<uint32_t>(varId) < varWriteStampCount_) {
                            uint64_t pubTsNs =
                                static_cast<uint64_t>(timeStamp.tv_sec()) * 1000000000ULL
                                + timeStamp.tv_nsec();
                            varWriteStamps_[varId].timestampNs.store(pubTsNs,
                                                                     std::memory_order_release);
                            varWriteStamps_[varId].blobType.store(static_cast<uint32_t>(blobType),
                                                                  std::memory_order_release);
                            varWriteStamps_[varId].writeCount.fetch_add(1,
                                                                        std::memory_order_release);
                        }
                        ++written;
                    } else {
                        ONDEMANDLOG_TIME(error, 5)
                            << "Failed to write varId: " << varId << " for varHash: " << varHash;
                    }
                }
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
                    /*1.拆表*/
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
                        meta.dataSize = 32;
                        meta.bucketIndex = bucketIdx;
                        meta.varId = varStore_.register_var(varHash, 32);

                        varIndex_.emplace(varHash, std::move(meta));
                        newBucketIds.insert(static_cast<uint32_t>(bucketIdx));
                        totalReceived_.fetch_add(1); // 记录收到的总数
                        ONDEMANDLOG(debug) << "Registered var: " << varName;
                    }
                    lock.unlock();

                    // 通知外层同步 define 到 Var::state
                    if (tableDefineCb_) {
                        std::vector<DSF::Var::Define> defines;
                        defines.reserve(tableDefine->varDefines().size());
                        for (const auto &varDef : tableDefine->varDefines()) {
                            defines.push_back(varDef.var().varDefine());
                        }
                        tableDefineCb_(defines);
                    }

                    /*初始化内存: register_var 只写元数据，必须 finalize 才分配 arena/dirty_flags*/
                    if (!varStore_.finalize()) {
                        ONDEMANDLOG(error) << "Failed to finalize VarStore after TableDefine";
                    }

                    /*同步扩容写入时间戳数组  暂时用一个单独数据记录时间戳，为了检测变化*/
                    {
                        uint32_t newCount = varStore_.var_count();
                        if (newCount > varWriteStampCount_) {
                            auto newStamps = std::make_unique<VarWriteStamp[]>(newCount);
                            for (uint32_t i = 0; i < varWriteStampCount_; ++i) {
                                newStamps[i].timestampNs.store(
                                    varWriteStamps_[i].timestampNs.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
                                newStamps[i].writeCount.store(
                                    varWriteStamps_[i].writeCount.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
                            }
                            varWriteStamps_ = std::move(newStamps);
                            varWriteStampCount_ = newCount;
                        }
                    }

                    /*统一检查是否有新 bucket 需要创建 reader，避免循环内逐次加锁*/
                    bool hasNewBucket = false;
                    if (!newBucketIds.empty()) {
                        std::lock_guard<std::mutex> mapLock(dataTransferCtxMapMutex_);
                        for (uint32_t bid : newBucketIds) {
                            if (dataTransferReaderMap_.find(bid) == dataTransferReaderMap_.end()) {
                                hasNewBucket = true;
                                break;
                            }
                        }
                    }
                    if (hasNewBucket) {
                        createDataTransferReader(std::bind(&OnDemandSub::onReceiveDataTransferCb,
                                                           this, std::placeholders::_1,
                                                           std::placeholders::_2));
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
        if (running_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already running";
            return false;
        }

        processTableDefineThread_ = std::thread(&OnDemandSub::processTableDefine, this);
        processDataTransferThread_ = std::thread(&OnDemandSub::processDataTransfer, this);

        /*启动回调调度器 (tick 精度 1ms, 线程池 8 线程)*/
        callbackScheduler_ = std::make_unique<TimerScheduler>(1, 8);
        /*调度函数*/
        callbackSchedulerThread_ = std::thread(&OnDemandSub::processCallbackScheduler, this);

        ONDEMANDLOG(info) << "OnDemandSub started";
        return true;
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

        /*先取消所有回调定时器*/
        cancelAllCallbackTimers();

        if (processTableDefineThread_.joinable()) {
            processTableDefineThread_.join();
        }
        if (processDataTransferThread_.joinable()) {
            processDataTransferThread_.join();
        }
        if (callbackSchedulerThread_.joinable()) {
            callbackSchedulerThread_.join();
        }

        /*停止回调调度器*/
        if (callbackScheduler_) {
            callbackScheduler_->Stop();
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

        dataNode_.reset();
        dataNode_ = nullptr;

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

        varWriteStamps_.reset();
        ONDEMANDLOG(info) << "OnDemandSub stopped";
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
        if (!initialized_) {
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

        /*2.开始组包*/
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

        /*3. 存储回调信息到本地, 供时间轮调度器使用*/
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
        if (!initialized_) {
            ONDEMANDLOG(error) << "OnDemandSub not initialized";
            return false;
        }
        std::string tableName;
        DSF::Message::SubTableRegister subReq;
        {
            std::shared_lock lock(varIndexMutex_);

            for (const auto &item : items) {
                DSF::NamedValue varFreq;
                /*1.计算点hash*/
                std::string metaVarName = make_meta_varname(node_name, item);
                uint64_t varHash = fast_hash(metaVarName);
                ONDEMANDLOG(debug) << "Subscribing to var: " << item << " with hash: " << varHash;
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

        /*2.开始组包*/
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

        /*3. 移除本地回调信息*/
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

            /* 只在订阅信息有变更时才重建分组 (dirty flag) */
            if (!callbackDirty_.exchange(false, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(kScanInterval);
                continue;
            }

            /* 1: 从 subscriptionCallbacks_ + varIndex_ 构建期望分组 */
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
                        /* 变量定义尚未到达, 暂时跳过*/
                        callbackDirty_.store(true, std::memory_order_release);
                        continue;
                    }

                    int32_t varId = vit->second.varId;
                    if (varId < 0) {
                        continue;
                    }

                    CallbackGroupKey key{cbInfo.freqMs};
                    auto &vec = desired[key];
                    if (!vec) {
                        vec = std::make_shared<std::vector<CallbackVarInfo>>();
                    }
                    CallbackVarInfo vi;
                    vi.varHash = varHash;
                    vi.varId = varId;
                    vi.dataSize = vit->second.dataSize;
                    vi.varName = cbInfo.varName;
                    vi.callback = cbInfo.callback;
                    vec->push_back(std::move(vi));
                }
            }

            /* 2: 增量 diff */
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
                    /* 始终刷新成员列表 */
                    callbackGroupMembers_[key] = std::move(members);

                    /* 仅为新增分组创建定时器，已有分组的定时器保持不变 */
                    if (callbackGroupTimers_.find(key) == callbackGroupTimers_.end()) {
                        uint32_t freqMs = key.freqMs;
                        Tick intervalTicks = static_cast<Tick>(freqMs);
                        auto timer = callbackScheduler_->ScheduleRecurring(
                            [this, freqMs]() { callbackGroupData(freqMs); },
                            intervalTicks, /* 首次延迟 */
                            intervalTicks  /* 周期 */
                        );
                        callbackGroupTimers_[key] = timer;
                        ONDEMANDLOG(debug) << "Created callback group: freq=" << freqMs
                                           << "ms, members=" << callbackGroupMembers_[key]->size();
                    }
                }
            }

            std::this_thread::sleep_for(kScanInterval);
        }
    }

    /**
     * @brief 回调分组数据: 读取 VarStore 并调用用户注册的 DataCallback
     * @param freqMs 回调频率 (ms), 用于定位分组
     */
    void OnDemandSub::callbackGroupData(uint32_t freqMs)
    {
        /* 获取组成员快照 */
        std::shared_ptr<std::vector<CallbackVarInfo>> members;
        {
            std::lock_guard<std::mutex> lock(callbackGroupsMutex_);
            CallbackGroupKey key{freqMs};
            auto it = callbackGroupMembers_.find(key);
            if (it == callbackGroupMembers_.end() || !it->second || it->second->empty()) {
                return;
            }
            members = it->second;
        }

        /* 预分配 flat buffer, 避免逐变量分配 */
        size_t totalBufSize = 0;
        for (const auto &info : *members)
            totalBufSize += info.dataSize;
        std::vector<uint8_t> dataBuf(totalBufSize);
        size_t offset = 0;

        DataCallback *groupCallback = nullptr;
        std::vector<VarCallbackData> batch;
        batch.reserve(members->size());

        for (auto &info : *members) {
            if (info.dataSize == 0 || !info.callback)
                continue;

            /* 检测数据时效 */
            uint32_t curWriteCount = 0;
            uint64_t tsNs = 0;
            DSF::Var::BLOB_TYPE blobType = DSF::Var::BLOB_TYPE::UNKNOWN;
            if (static_cast<uint32_t>(info.varId) < varWriteStampCount_) {
                curWriteCount =
                    varWriteStamps_[info.varId].writeCount.load(std::memory_order_acquire);
                if (curWriteCount == info.lastSeenWriteCount)
                    continue;
                tsNs = varWriteStamps_[info.varId].timestampNs.load(std::memory_order_acquire);
                blobType = static_cast<DSF::Var::BLOB_TYPE>(
                    varWriteStamps_[info.varId].blobType.load(std::memory_order_acquire));
            }

            uint8_t *ptr = dataBuf.data() + offset;
            if (!varStore_.read(info.varId, ptr))
                continue;

            info.lastSeenWriteCount = curWriteCount;
            if (!groupCallback)
                groupCallback = &info.callback;
            batch.push_back({info.varName, ptr, info.dataSize, tsNs, blobType});
            offset += info.dataSize;
        }

        if (batch.empty() || !groupCallback)
            return;
        try {
            (*groupCallback)(batch);
        } catch (const std::exception &e) {
            ONDEMANDLOG_TIME(error, 5000) << "Batch callback exception for freq=" << freqMs
                                          << "ms, vars=" << batch.size() << " err: " << e.what();
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

} // namespace ondemand
} // namespace dsf
