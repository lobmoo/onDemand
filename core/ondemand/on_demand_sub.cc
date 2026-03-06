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
                        ++written;
                    } else {
                        ONDEMANDLOG_TIME(error, 5)
                            << "Failed to write varId: " << varId << " for varHash: " << varHash;
                    }
                }
            }

            ONDEMANDLOG(critical) << "DataTransfer: received=" << varDataList.size()
                                  << " written=" << written;
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
                        meta.bucketIndex = bucketIdx;
                        meta.varId = varStore_.register_var(varHash, 32);

                        varIndex_.emplace(varHash, std::move(meta));
                        newBucketIds.insert(static_cast<uint32_t>(bucketIdx));
                        totalReceived_.fetch_add(1); // 记录收到的总数
                        ONDEMANDLOG(debug) << "Registered var: " << varName;
                    }
                    lock.unlock();

                    /*初始化内存: register_var 只写元数据，必须 finalize 才分配 arena/dirty_flags*/
                    if (!varStore_.finalize()) {
                        ONDEMANDLOG(error) << "Failed to finalize VarStore after TableDefine";
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
                        createDataTransferReader(std::bind(
                            &OnDemandSub::onReceiveDataTransferCb, this, std::placeholders::_1,
                            std::placeholders::_2)); //TODO: 传入实际的数据处理回调函数
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
        if (processTableDefineThread_.joinable()) {
            processTableDefineThread_.join();
        }
        if (processDataTransferThread_.joinable()) {
            processDataTransferThread_.join();
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
        ONDEMANDLOG(info) << "OnDemandSub stopped";
    }

    /**
     * @brief 订阅变量
     * @param  node_name 节点名
     * @param  items 变量信息列表
     * @return true 成功
     * @return false 失败
     */
    bool OnDemandSub::subscribe(const char *node_name, const std::vector<SubscriptionItem> &items)
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
                    ONDEMANDLOG(warning) << "Variable not found for subscription: " << item.varName;
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

        return true;
    }

    void OnDemandSub::onWriterDiscovery(const DdsWrapper::EndpointInfo &info)
    {
        ONDEMANDLOG(debug) << "[sub node]Writer discovery: topic=" << info.topic_name
                           << " type=" << info.type_name << " discovered=" << info.discovered;
    }

    // size_t OnDemandSub::getSubscriptionCount() const
    // {
    //     std::shared_lock lock(subMutex_);
    //     return subscriptions_.size();
    // }

} // namespace ondemand
} // namespace dsf
