/**
 * @file on_demand_pub_v2.cpp
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2026-02-05
 * 
 * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2026-02-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */

#include "on_demand_pub.h"
#include <charconv>

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

    bool OnDemandPub::init(const std::string &nodeName)
    {
        if (initialized_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already initialized";
            return false;
        }

        nodeName_ = nodeName;

        /*创建节点*/
        try {
            dataNode_ = std::make_shared<DdsWrapper::DataNode>(DOMAIN_ID, nodeName);
        } catch (const std::exception &e) {
            ONDEMANDLOG(error) << "Failed to create DataNode: " << e.what();
            return false;
        }

        /*创建变量通知topic writer*/
        if (!createTableDefineWriter()) {
            return false;
        }

        /*创建接收频率请求topic reader*/
        if (!createSubTableRegisterReader(std::bind(&OnDemandPub::onReceiveRegisterCb, this,
                                                    std::placeholders::_1,
                                                    std::placeholders::_2))) {
            return false;
        }

        ONDEMANDLOG(info) << "OnDemandPub initialized: " << nodeName;
        return true;
    } // namespace ondemand

    bool OnDemandPub::start()
    {
        if (running_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already running";
            return false;
        }

        registerProcessThread_ = std::thread(&OnDemandPub::processReceiveRegister, this);
        ONDEMANDLOG(info) << "OnDemandPub started";
        return true;
    }

    void OnDemandPub::stop()
    {
        initialized_.store(false);
        if (!running_.exchange(false)) {
            return;
        }
        if (registerProcessThread_.joinable()) {
            registerProcessThread_.join();
        }

        std::unique_lock lock(varIndexMutex_);
        varIndex_.clear();
        bucketManager_.Clear();

        pubTableDefineWriter_.reset();
        pubTableDefineWriter_ = nullptr;
        subTableRegisterReqReader_.reset();
        subTableRegisterReqReader_ = nullptr;
        {
            std::lock_guard<std::mutex> lock(DataTransferWriterMapMutex_);
            dataTransferWriterMap_.clear();
        }
        dataNode_.reset();
        dataNode_ = nullptr;

        ONDEMANDLOG(info) << "OnDemandPub stopped";
    }

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

    bool OnDemandPub::createDataTransferWriter()
    {
        // 根据 varIndex_ 中的元数据，收集所有不同的 bucket id
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

        constexpr uint32_t depth = 1;
        DdsWrapper::DataWriterQoSBuilder writerQosBuilder;
        // writerQosBuilder.setMaxSamples(32 * depth)
        //     .setMaxInstances(32)
        //     .setMaxSamplesPerInstance(depth)
        //     .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        //     .setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        //     .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        //     .setHistoryDepth(depth);

        std::lock_guard<std::mutex> lock(DataTransferWriterMapMutex_);

        for (uint32_t bucketId : bucketIds) {
            // 如果该 bucket 的 writer 已存在，跳过
            if (dataTransferWriterMap_.find(bucketId) != dataTransferWriterMap_.end()) {
                ONDEMANDLOG(debug) << "DataTransfer writer already exists for bucketId: " << bucketId;
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

    bool OnDemandPub::tableDefinePublish(const DSF::Var::PubTableDefine &pubTableDefine)
    {
        if (pubTableDefineWriter_ != nullptr) {
            return pubTableDefineWriter_->writeMessage(pubTableDefine);
        } else {
            ONDEMANDLOG(error) << "pubTableDefineWriter_ is nullptr";
            return false;
        }
    }

    bool OnDemandPub::onReceiveRegisterCb(const std::string &topicName,
                                          std::shared_ptr<DSF::Message::SubTableRegister> data)
    {
        pubTableDefRegisterQueue_.enqueue(data);
        return true;
    }

    void OnDemandPub::processReceiveRegister()
    {
        while (running_.load()) {
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

                    // [DEBUG] 打印相关变量的 VarMetadata
                    {
                        std::shared_lock dumpLock(varIndexMutex_);
                        for (const auto &varFreq : data->varFreqs()) {
                            std::string metaName = varFreq.name();
                            uint64_t varHash = fast_hash(metaName);
                            auto it = varIndex_.find(varHash);
                            if (it != varIndex_.end()) {
                                it->second.dump(varFreq.name());
                            }
                        }
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    bool OnDemandPub::createVars(const std::vector<DSF::Var::Define> &VarDefines)
    {
        std::unique_lock lock(varIndexMutex_);
        // Reserve space to avoid multiple reallocations
        varIndex_.reserve(varIndex_.size() + VarDefines.size());

        for (const auto &VarDefine : VarDefines) {
            const auto &varName = make_meta_varname(nodeName_, VarDefine.name());
            uint64_t varHash = fast_hash(varName);
            size_t bucketIdx = BucketManager::CalculateBucketIndexFromHash(varHash); // Reuse hash

            VarMetadata meta;
            meta.varHash = varHash;
            meta.currentFreq = 0xFFFFFFFF;
            meta.activeFreqCount = 0;
            meta.bucketIndex = bucketIdx;
            meta.varDefine = std::make_shared<DSF::Var::Define>(VarDefine);
            auto it = varIndex_.find(varHash);
            if (it != varIndex_.end()) {
                ONDEMANDLOG(warning) << "Variable already exists: " << varName;
                continue;
            }
            meta.varId = varStore_.register_var(varHash, 32); //todo   这里应该按照真实大小分配内存
            varIndex_.emplace(varHash, std::move(meta));
            bucketManager_.AddMember(varName, varHash);
        }

        /*初始化内存*/
        if (!varStore_.finalize()) {
            ONDEMANDLOG(error) << "Failed to finalize variable store";
            return false;
        }

        uint32_t bucketCount = bucketManager_.GetBucketCount();
        for (uint32_t i = 0; i < bucketCount; ++i) {
            // Skip empty buckets
            if (bucketManager_.GetBucketSize(i) == 0) {
                continue;
            }

            DSF::Var::PubTableDefine pubTableDefine;
            pubTableDefine.name(make_bucket_name_by_id(i));
            pubTableDefine.nodeName(nodeName_);
            pubTableDefine.description("onDemandPub TableDefine");

            /*给每个表下面的每个变量赋值*/
            const auto members = bucketManager_.GetBucketMembers(i);
            // Reserve space to avoid multiple reallocations
            pubTableDefine.varDefines().reserve(members.size());

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

            ONDEMANDLOG(info) << "Publishing table define for bucket " << i << " with "
                              << pubTableDefine.varDefines().size() << " variables";
            if (!pubTableDefine.varDefines().empty()) {
                tableDefinePublish(pubTableDefine);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ONDEMANDLOG(info) << "Published bucket " << i + 1 << "/" << bucketCount << " with "
                                  << pubTableDefine.varDefines().size() << " variables";
            }
        }

        /*创建数据传输writer*/
        if (!createDataTransferWriter()) {
            ONDEMANDLOG(error) << "Failed to create DataTransfer writers";
            return false;
        }

        return true;
    }

    bool OnDemandPub::deleteVars(const std::vector<std::string> &varNames)
    {
        std::unique_lock lock(varIndexMutex_);

        for (const auto &varName : varNames) {
            uint64_t varHash = fast_hash(make_meta_varname(nodeName_, varName));
            auto it = varIndex_.find(varHash);
            if (it == varIndex_.end()) {
                ONDEMANDLOG(warning) << "Variable not found: " << varName;
                continue;
            }

            varIndex_.erase(it);
            bucketManager_.RemoveMember(varName, varHash);
        }

        uint32_t bucketCount = bucketManager_.GetBucketCount();
        for (uint32_t i = 0; i < bucketCount; ++i) {

            DSF::Var::PubTableDefine pubTableDefine;
            pubTableDefine.name(make_bucket_name_by_id(i));
            pubTableDefine.nodeName(nodeName_);
            pubTableDefine.description("onDemandPub TableDefine");

            /*给每个表下面的每个变量赋值*/
            const auto members = bucketManager_.GetBucketMembers(i);
            // Reserve space to avoid multiple reallocations
            pubTableDefine.varDefines().reserve(members.size());

            for (const auto &varName : members) {
                uint64_t varHash = fast_hash(varName);
                varStore_.unregister_var(varHash);
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

            // 差分删除：即使是空表也要发布，通知订阅者该表已清空
            tableDefinePublish(pubTableDefine);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ONDEMANDLOG(info) << "Published bucket " << i + 1 << "/" << bucketCount << " with "
                              << pubTableDefine.varDefines().size() << " variables (delete mode)";
        }

        return true;
    }

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

            ONDEMANDLOG(info) << "Var [" << varFreq.name() << "] subscribed by node [" << nodeName
                              << "] at freq=" << freq << "ms, currentFreq=" << meta.currentFreq
                              << "ms";
        }
    }

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

            ONDEMANDLOG(info) << "Var [" << varFreq.name() << "] unsubscribed by node [" << nodeName
                              << "], freq " << oldFreq << "ms -> " << meta.currentFreq << "ms";
        }
    }

    // void OnDemandPub::dumpState(std::ostream &os)
    // {
    //     os << "========== OnDemandPub State ==========\n";
    //     os << "Node: " << nodeName_ << "\n";
    //     os << "Running: " << (running_ ? "Yes" : "No") << "\n";

    //     std::shared_lock lock(varIndexMutex_);
    //     os << "Total Variables: " << varIndex_.size() << "\n";

    //     os << "==============================================\n";
    // }

    // size_t OnDemandPub::getMemoryUsage() const
    // {
    //     std::shared_lock lock(varIndexMutex_);
    //     size_t total = varIndex_.size() * sizeof(VarMetadata);
    //     total += varIndex_.size() * (sizeof(uint64_t) + sizeof(void *));
    //     return total;
    // }

} // namespace ondemand
} // namespace dsf
