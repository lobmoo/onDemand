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
          subTableRegisterReqReader_(nullptr)
    {
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

        DdsWrapper::ParticipantQoSBuilder qos_configurator;
        qos_configurator.addUDPV4TransportInterfaces({"10.25.5.26"})
            .setDiscoveryMulticastLocator("239.255.0.1", 7400)
            .setUserMulticastLocator("239.255.0.1", 7401)
            .addFlowController()
            .setDiscoveryKeepAlive(2000, 500)
            .setInitialAnnouncements(30, 100); // 10次PDP公告, 100ms间隔, 确保3秒内完成初始发现
        /*创建节点*/
        try {
            dataNode_ =
                std::make_shared<DdsWrapper::DataNode>(DOMAIN_ID, nodeName, qos_configurator, this);
        } catch (const std::exception &e) {
            ONDEMANDLOG(error) << "Failed to create DataNode: " << e.what();
            initialized_.store(false);
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
        if (running_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already running";
            return false;
        }

        /*创建时间轮调度器: 1ms tick 精度*/
        const size_t poolSize = 8;
        publishScheduler_ = std::make_unique<TimerScheduler>(1, poolSize);

        registerProcessThread_ = std::thread(&OnDemandPub::processReceiveRegister, this);
        publishSchedulerThread_ = std::thread(&OnDemandPub::processPublishTaskScheduler, this);

        ONDEMANDLOG(info) << "OnDemandPub started (TimerScheduler: 1ms tick, pool=" << poolSize
                          << ")";
        return true;
    }

    /**
     * @brief 停止发布者节点，清理资源，停止所有定时器和线程
     */
    void OnDemandPub::stop()
    {
        initialized_.store(false);
        bool wasRunning = running_.exchange(false);

        if (wasRunning) {
            /*等待线程退出*/
            if (registerProcessThread_.joinable()) {
                registerProcessThread_.join();
            }
            if (publishSchedulerThread_.joinable()) {
                publishSchedulerThread_.join();
            }

            /*清理所有发布定时器和调度器*/
            cancelAllPublishTimers();
            if (publishScheduler_) {
                publishScheduler_->Stop();
                publishScheduler_.reset();
            }
        }

        /*清理 varIndex_ 和 bucketManager_ (受 varIndexMutex_ 保护)*/
        {
            std::unique_lock lock(varIndexMutex_);
            varIndex_.clear();
            bucketManager_.Clear();
        }

        /*清理 DDS 读写器和节点*/
        pubTableDefineWriter_.reset();
        subTableRegisterReqReader_.reset();
        {
            std::lock_guard<std::mutex> lock(DataTransferWriterMapMutex_);
            dataTransferWriterMap_.clear();
        }
        dataNode_.reset();

        ONDEMANDLOG(info) << "OnDemandPub stopped";
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
            if (0
                != dsf::ondemand::registerNodeTopicWriter<DSF::Var::TableDataTransfer,
                                                          DSF::Var::TableDataTransferPubSubType>(
                    dataNode_, writer, topicName, writerQosBuilder)) {
                ONDEMANDLOG(error)
                    << "Failed to create DataTransfer writer for topic: " << topicName;
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
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::onReceiveRegisterCb(const std::string & /*topicName*/,
                                          std::shared_ptr<DSF::Message::SubTableRegister> data)
    {
        pubTableDefRegisterQueue_.enqueue(data);
        return true;
    }

    /**
     * @brief 处理订阅者注册请求
     */
    void OnDemandPub::processReceiveRegister()
    {
        while (running_.load(std::memory_order_acquire)) {
            std::shared_ptr<DSF::Message::SubTableRegister> data;
            if (pubTableDefRegisterQueue_.try_dequeue(data)) {
                if (data) {
                    ONDEMANDLOG(info)
                        << "Received subscription register from node: " << data->nodeName()
                        << " with " << data->varFreqs().size() << " variables";

                    // 处理订阅注册请求
                    switch (data->msgType()) {
                        case DSF::Message::MSGTYPE::SUB_TABLE_REGISTER:
                            handleSubscribe(data->nodeName(), data->varFreqs());
                            break;
                        case DSF::Message::MSGTYPE::SUB_TABLE_UNREGISTER:
                            handleUnsubscribe(data->nodeName(), data->varFreqs());
                            break;
                        default:
                            LOG(error) << "Unknown registered type.";
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
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    /**
     * @brief 发布调度监控线程
     *
     * 增量策略:
     *   - 频率未变 → 定时器不动，仅刷新组成员列表
     *   - 新增分组 → 创建定时器
     *   - 移除分组 → 取消定时器
     *   - 10w 变量同频 →  20 bucket 天然分散, 无需额外分片
     */
    void OnDemandPub::processPublishTaskScheduler()
    {
        pthread_setname_np(pthread_self(), "PubScheduler");
        constexpr auto kScanInterval = std::chrono::milliseconds(50);

        while (running_.load(std::memory_order_acquire)) {

            // 只在 varIndex_ 有变更时才重建分组 (dirty flag)
            if (!schedulerDirty_.exchange(false, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(kScanInterval);
                continue;
            }

            //1: 从 varIndex_ 构建期望分组
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
                    /*<id+freq, varInfo>*/
                    vec->push_back(GroupVarInfo{varHash, meta.varId, meta.dataSize});
                }
            }

            // 预排序: 按 varHash 升序, 与 Roaring64Map 迭代顺序一致
            for (auto &[gkey, vec] : desired) {
                if (vec) {
                    std::sort(vec->begin(), vec->end(),
                              [](const GroupVarInfo &a, const GroupVarInfo &b) {
                                  return a.varHash < b.varHash;
                              });
                }
            }

            //增量 diff ----
            {
                std::lock_guard<std::mutex> lock(publishGroupsMutex_);
                // 新的一轮扫描发现分配的key在原来的时间轮分组里面没有，移除不再需要的分组
                for (auto it = publishGroupTimers_.begin(); it != publishGroupTimers_.end();) {
                    if (desired.find(it->first) == desired.end()) {
                        if (publishScheduler_) {
                            publishScheduler_->Cancel(it->second);
                        }
                        groupMembers_.erase(it->first);
                        ONDEMANDLOG(debug)
                            << "Removed publish group: bucket=" << it->first.bucketIndex
                            << " freq=" << it->first.freqMs << "ms";
                        it = publishGroupTimers_.erase(it);
                    } else {
                        ++it;
                    }
                }

                for (auto &[key, members] : desired) {
                    // 始终刷新成员列表
                    groupMembers_[key] = std::move(members);

                    // 仅为新增分组创建定时器，已有分组的定时器保持不变
                    if (publishGroupTimers_.find(key) == publishGroupTimers_.end()) {
                        uint32_t bucketIdx = key.bucketIndex;
                        uint32_t freqMs = key.freqMs;
                        Tick intervalTicks = static_cast<Tick>(freqMs);
                        auto timer = publishScheduler_->ScheduleRecurring(
                            [this, bucketIdx, freqMs]() { publishGroupData(bucketIdx, freqMs); },
                            intervalTicks, // 首次延迟
                            intervalTicks  // 周期
                        );
                        publishGroupTimers_[key] = timer;
                        ONDEMANDLOG(debug)
                            << "Created publish group: bucket=" << bucketIdx << " freq=" << freqMs
                            << "ms, members=" << groupMembers_[key]->size();
                    }
                }
            }

            std::this_thread::sleep_for(kScanInterval);
        }
    }

    /**
     * @brief 批量发布组内变量数据 (定时器回调, 线程池执行)
     *
     * 性能优化:
     *   - groupMembers_ 已按 varHash 预排序, 无需发送时再排序
     *   - 直接构建 msg, 省去中间 varDataList 容器分配
     *   - Roaring64Map 压缩 mask
     *
     * @param bucketIndex  桶索引 (映射到 DDS topic)
     * @param freqMs       频率 (ms), 用于定位分组
     */
    void OnDemandPub::publishGroupData(uint32_t bucketIndex, uint32_t freqMs)
    {
        // 获取组成员快照 (已按 varHash 升序预排序)
        std::shared_ptr<std::vector<GroupVarInfo>> members;
        {
            std::lock_guard<std::mutex> lock(publishGroupsMutex_);
            PublishGroupKey key{bucketIndex, freqMs};
            auto it = groupMembers_.find(key);
            if (it == groupMembers_.end() || !it->second || it->second->empty()) {
                return;
            }
            members = it->second;
        }

        // 直接构建消息, 避免中间 varDataList 分配
        DSF::Var::TableDataTransfer msg;
        roaring::Roaring64Map roar;
        msg.varData().reserve(members->size());

        for (const auto &info : *members) {
            if (info.dataSize == 0)
                continue;

            std::vector<uint8_t> buf(info.dataSize);
            if (!varStore_.read(info.varId, buf.data())) {
                continue;
            }
            roar.add(info.varHash);
            msg.varData().emplace_back(std::move(buf));
        }

        if (msg.varData().empty()) {
            return;
        }

        // 序列化 Roaring64Map -> mask
        roar.runOptimize();
        roar.shrinkToFit();
        size_t roarBytes = roar.getSizeInBytes();
        msg.mask().resize(roarBytes);
        roar.write(reinterpret_cast<char *>(msg.mask().data()));

        // 时间戳
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(epoch);
        auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch) - sec;
        msg.timestamp().tv_sec(static_cast<int32_t>(sec.count()));
        msg.timestamp().tv_nsec(static_cast<uint32_t>(nsec.count()));
        msg.blobType(DSF::Var::BLOB_TYPE::STRUCTS);

        // 发送
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
            LOG(critical) << "+++++++++++++++++++++++++pub success+++++++++++++++++++++";
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

        ONDEMANDLOG(info) << "All publish group timers canceled";
    }

    /**
     * @brief 创建变量并发布表定义，支持增量更新
     * @param  VarDefines 变量定义列表，包含变量名称、类型、频率等信息
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandPub::createVars(const std::vector<DSF::Var::Define> &VarDefines)
    {
        // Phase 1: 注册变量 + 初始化存储 (写锁, 快速完成)
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
                constexpr uint32_t kDefaultVarSize = 32; // TODO: 按真实大小分配
                meta.dataSize = kDefaultVarSize;
                meta.varId = varStore_.register_var(varHash, kDefaultVarSize);
                varIndex_.emplace(varHash, std::move(meta));
                bucketManager_.AddMember(varName, varHash);
            }

            if (!varStore_.finalize()) {
                ONDEMANDLOG(error) << "Failed to finalize variable store";
                return false;
            }
        } // 释放写锁

        // Phase 2: 创建 DataTransfer writers (在发布 TableDefine 之前准备好)
        if (!createDataTransferWriter()) {
            ONDEMANDLOG(error) << "Failed to create DataTransfer writers";
            return false;
        }

        // Phase 3: 发布 TableDefine (读锁, 不阻塞 setVarData 等热路径)
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

            ONDEMANDLOG(info) << "Publishing table define for bucket " << i << " with "
                              << pubTableDefine.varDefines().size() << " variables";
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
        // Phase 1: 注销被删变量 (写锁)
        {
            std::unique_lock lock(varIndexMutex_);
            for (const auto &varName : varNames) {
                uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varName));
                auto it = varIndex_.find(varHash);
                if (it == varIndex_.end()) {
                    ONDEMANDLOG(warning) << "Variable not found: " << varName;
                    continue;
                }
                varStore_.unregister_var(varHash);
                varIndex_.erase(it);
                bucketManager_.RemoveMember(varName, varHash);
            }
        } // 释放写锁

        schedulerDirty_.store(true, std::memory_order_release);

        // Phase 2: 发布更新后的 TableDefine (读锁, 不阻塞热路径)
        uint32_t bucketCount = bucketManager_.GetBucketCount();
        for (uint32_t i = 0; i < bucketCount; ++i) {
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

            // 差分删除: 即使是空表也要发布, 通知订阅者该表已清空
            tableDefinePublish(pubTableDefine);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ONDEMANDLOG(info) << "Published bucket " << i + 1 << "/" << bucketCount << " with "
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
        int32_t varId = -1;
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

    /**
     * @brief 获取或分配订阅者节点的位图位置，支持最多 256 个订阅者
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
            ONDEMANDLOG(error) << "Node slot overflow! Max 64 subscriber nodes supported.";
            return 63; // 饱和处理，共享最后一个 bit
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
    void OnDemandPub::handleSubscribe(const std::string &nodeName,
                                      const std::vector<DSF::NamedValue> &varFreqs)
    {
        uint64_t nodeHash = fast_hash(nodeName);

        std::unique_lock lock(varIndexMutex_);
        uint8_t nodeBit = getOrAssignNodeBit(nodeHash);
        uint64_t nodeMask = uint64_t(1) << nodeBit;

        for (const auto &varFreq : varFreqs) {
            std::string metaName = varFreq.name(); // 这里注册请求已经是全名了，不需要再拼接一次了
            uint64_t varHash = fast_hash(metaName);
            auto it = varIndex_.find(varHash);
            if (it == varIndex_.end()) {
                LOG(warning) << "register var not found ! var name: " << varFreq.name()
                             << " node name: " << nodeName;
                continue;
            }

            auto &meta = it->second;

            // 解析频率
            uint32_t freq;
            auto result = std::from_chars(varFreq.value().data(),
                                          varFreq.value().data() + varFreq.value().size(), freq);
            if (std::errc() != result.ec) {
                ONDEMANDLOG(warning) << "Invalid frequency value for var: " << varFreq.name()
                                     << " node: " << nodeName << " value: " << varFreq.value();
                continue;
            }

            // 先从该节点已有的其他频率条目中移除（一个节点对一个变量只保留一个频率）
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

            // 设置该节点的订阅位
            target->subMask |= nodeMask;
            target->subCount++;

            // 更新活跃频率数量
            meta.activeFreqCount = static_cast<uint8_t>(meta.freqSubs.size());

            // 重新计算最小发布频率
            recalcCurrentFreq(meta);
            schedulerDirty_.store(true, std::memory_order_release);

            ONDEMANDLOG(info) << "Var [" << varFreq.name() << "] subscribed by node [" << nodeName
                              << "] at freq=" << freq << "ms, currentFreq=" << meta.currentFreq
                              << "ms";
        }
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

        std::unique_lock lock(varIndexMutex_);

        // 查找该节点的 bit 位
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
                LOG(warning) << "unregister var not found ! var name: " << varFreq.name()
                             << " node name: " << nodeName;
                continue;
            }

            auto &meta = it->second;

            // 从 freqSubs 中移除该节点的订阅
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

            // 更新活跃频率数量
            meta.activeFreqCount = static_cast<uint8_t>(meta.freqSubs.size());

            // 重新计算最小频率（关键：退订后自动切换到下一个最小频率）
            uint32_t oldFreq = meta.currentFreq;
            recalcCurrentFreq(meta);
            schedulerDirty_.store(true, std::memory_order_release);

            ONDEMANDLOG(info) << "Var [" << varFreq.name() << "] unsubscribed by node [" << nodeName
                              << "], freq " << oldFreq << "ms -> " << meta.currentFreq << "ms";
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

} // namespace ondemand
} // namespace dsf
