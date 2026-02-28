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

namespace dsf
{
namespace ondemand
{

    OnDemandPub::OnDemandPub()
        : varIndex_(), varIndexMutex_(), bucketManager_(), initialized_(false), running_(false),
          dataNode_(nullptr), nodeName_(), pubTableDefineWriter_(nullptr),
          subTableRegisterReqReader_(nullptr), subTableRegisterRespWriter_(nullptr)
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
        if (!createSubTableRegisterReader(std::bind(&OnDemandPub::onReceiveRegisterCb, this, std::placeholders::_1, std::placeholders::_2))) {
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
        if(registerProcessThread_.joinable()) {
            registerProcessThread_.join();
        }

        std::unique_lock lock(varIndexMutex_);
        varIndex_.clear();
        bucketManager_.Clear();

        pubTableDefineWriter_.reset();
        pubTableDefineWriter_ = nullptr;
        subTableRegisterReqReader_.reset();
        subTableRegisterReqReader_ = nullptr;
        subTableRegisterRespWriter_.reset();
        subTableRegisterRespWriter_ = nullptr;
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
                    ONDEMANDLOG(info) << "Received subscription register from node: " << data->nodeName()
                                      << " with " << data->varFreqs().size() << " variables";
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

    // void OnDemandPub::handleSubscribe(const std::string &nodeName,
    //                                   const std::vector<std::pair<std::string, uint32_t>> &vars)
    // {
    //     uint64_t nodeHash = fast_hash(nodeName);

    //     std::unique_lock lock(varIndexMutex_);
    //     for (const auto &[varName, freq] : vars) {
    //         uint64_t varHash = fast_hash(varName);

    //         auto it = varIndex_.find(varHash);
    //         if (it == varIndex_.end()) {
    //             continue;
    //         }

    //         auto &meta = it->second;

    //         // 更新频率
    //         if (freq < meta.currentFreq) {
    //             meta.currentFreq = freq;
    //         }
    //     }

    //     ONDEMANDLOG(info) << "Node " << nodeName << " subscribed to " << vars.size() << " vars";
    // }

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
