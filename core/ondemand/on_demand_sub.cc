#include <functional>
#include "on_demand_sub.h"
#include "concurrentqueue.h"

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

    bool OnDemandSub::onReceiveTableDefineCb(const std::string &topicName,
                                             std::shared_ptr<DSF::Var::PubTableDefine> data)
    {
        pubTableDefineQueue_.enqueue(data);
        return true;
    }

    /**
     * @brief 处理变量定义
     */
    void OnDemandSub::processTableDefine()
    {
        pthread_setname_np(pthread_self(), "proc_tab_def");
        while (running_) {
            std::shared_ptr<DSF::Var::PubTableDefine> tableDefine;
            if (pubTableDefineQueue_.try_dequeue(tableDefine)) {
                if (tableDefine) {
                    ONDEMANDLOG(info) << "Processing TableDefine: " << tableDefine->name()
                                      << ", vars size: " << tableDefine->varDefines().size();
                    std::unique_lock lock(varIndexMutex_);
                    /*1.拆表*/
                    for (auto &varDef : tableDefine->varDefines()) {
                        const auto &varDefine = varDef.var().varDefine();
                        std::string varName = make_meta_varname(
                            varDefine.nodeName(), varDefine.name()); // 构造全名 和发布端一致
                        uint64_t varHash = fast_hash(varName);
                        size_t bucketIdx =
                            BucketManager::CalculateBucketIndexFromHash(varHash); // Reuse hash
                        VarMetadata meta;
                        meta.varHash = varHash;
                        meta.currentFreq = 0xFFFFFFFF;
                        meta.activeFreqCount = 0;
                        meta.bucketIndex = bucketIdx;
                        meta.varDefine = std::make_shared<DSF::Var::Define>(varDefine);
                        auto it = varIndex_.find(varHash);
                        if (it != varIndex_.end()) {
                            ONDEMANDLOG(warning) << "Variable already exists: " << varName;
                            continue;
                        }
                        meta.varId = varStore_.register_var(
                            varHash, 32); //todo   这里应该按照真实大小分配内存
                        varIndex_.emplace(varHash, std::move(meta));
                        totalReceived_.fetch_add(1); //记录收到的总数
                        ONDEMANDLOG(debug) << "Registered var: " << varName;
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    bool OnDemandSub::init(const std::string &nodeName)
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

        /*创建变量定义接收reader*/
        if (!createTableDefineReader(std::bind(&OnDemandSub::onReceiveTableDefineCb, this,
                                               std::placeholders::_1, std::placeholders::_2))) {
            ONDEMANDLOG(error) << "Failed to create TableDefine reader";
            return false;
        }

        /*定义变量注册writer*/
        if (!createSubTableRegisterWriter()) {
            ONDEMANDLOG(error) << "Failed to create SubTableRegister writer";
            return false;
        }

        ONDEMANDLOG(info) << "OnDemandSub initialized: " << nodeName;
        return true;
    }

    bool OnDemandSub::start()
    {
        if (running_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already running";
            return false;
        }

        processTableDefineThread_ = std::thread(&OnDemandSub::processTableDefine, this);
        ONDEMANDLOG(info) << "OnDemandSub started";
        return true;
    }

    void OnDemandSub::stop()
    {
        initialized_.store(false);
        if (!running_.exchange(false)) {
            return;
        }
        if (processTableDefineThread_.joinable()) {
            processTableDefineThread_.join();
        }
        pubTableDefineReader_.reset();
        pubTableDefineReader_ = nullptr;
        subTableRegisterReqWriter_.reset();
        subTableRegisterReqWriter_ = nullptr;
        dataNode_.reset();
        dataNode_ = nullptr;

        totalReceived_.store(0);

        std::shared_ptr<DSF::Var::PubTableDefine> dummy;
        while (pubTableDefineQueue_.try_dequeue(dummy)) {
            // Dequeue and discard remaining items
        }
        ONDEMANDLOG(info) << "OnDemandSub stopped";
    }

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
                DSF::NamedValue varFreq;
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
        if (!subTableRegisterReqWriter_->writeMessage(subReq)) {
            ONDEMANDLOG(error) << "Failed to publish SubTableRegister for table: " << tableName;
            return false;
        }

        return true;
    }

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
        if (!subTableRegisterReqWriter_->writeMessage(subReq)) {
            ONDEMANDLOG(error) << "Failed to publish SubTableRegister for table: " << tableName;
            return false;
        }

        return true;
    }

    // size_t OnDemandSub::getSubscriptionCount() const
    // {
    //     std::shared_lock lock(subMutex_);
    //     return subscriptions_.size();
    // }

    // void OnDemandSub::dumpState(std::ostream &os)
    // {
    //     os << "========== OnDemandSub State ==========\n";
    //     os << "Node: " << nodeName_ << "\n";
    //     os << "Running: " << (running_ ? "Yes" : "No") << "\n";
    //     os << "Total Received: " << totalReceived_.load() << "\n";

    //     std::shared_lock lock(subMutex_);
    //     os << "Active Subscriptions: " << subscriptions_.size() << "\n\n";

    //     os << "=== Subscriptions ===\n";
    //     for (const auto &[hash, item] : subscriptions_) {
    //         os << "  " << item.varName << " [" << item.tableName << "]"
    //            << " @ " << item.frequency << "ms\n";
    //     }

    //     os << "==============================================\n";
    // }

} // namespace ondemand
} // namespace dsf
