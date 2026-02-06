#include <functional>
#include "on_demand_sub.h"


namespace dsf
{
namespace ondemand
{

    OnDemandSub::OnDemandSub() : initialized_(false), running_(false), totalReceived_(0) {}

    OnDemandSub::~OnDemandSub() { stop(); }

    bool OnDemandSub::createTableDefineReader(
        std::function<void(const std::string &, std::shared_ptr<DSF::Var::PubTableDefine>)>
            processFunc)
    {

        constexpr uint32_t depth = 20;
        DdsWrapper::DataReaderQoSBuilder readerQosBuilder;
        readerQosBuilder.setMaxSamples(256 * depth)
            .setMaxInstances(256)
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

    bool OnDemandSub::onReceiveTableDefine(const std::string &topicName,
                                           std::shared_ptr<DSF::Var::PubTableDefine> data)
    {
        ONDEMANDLOG(info) << "topic: " << topicName
                          << ", TableDefine: " << data->name()
                          << ", vars: " << data->varDefines().size();
        return true;
    }

    bool OnDemandSub::init(const std::string &nodeName)
    {
        if (initialized_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already initialized";
            return false;
        }
        nodeName_ = nodeName;

        /*创建变量定义接收reader*/
        if (!createTableDefineReader(std::bind(&OnDemandSub::onReceiveTableDefine, this,
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

    // bool OnDemandSub::start()
    // {
    //     if (running_.exchange(true)) {
    //         ONDEMANDLOG(WARNING) << "Already running";
    //         return false;
    //     }

    //     ONDEMANDLOG(info) << "OnDemandSub started";
    //     return true;
    // }

    void OnDemandSub::stop()
    {
        if (!running_.exchange(false)) {
            return;
        }

        ONDEMANDLOG(info) << "OnDemandSub stopped";
    }

    // bool OnDemandSub::subscribe(const std::string &varName, const std::string &tableName,
    //                                      uint32_t frequency)
    // {
    //     SubscriptionItem item(varName, tableName, frequency);

    //     std::unique_lock lock(subMutex_);

    //     if (subscriptions_.find(item.varHash) != subscriptions_.end()) {
    //         ONDEMANDLOG(WARNING) << "Already subscribed: " << varName;
    //         return false;
    //     }

    //     subscriptions_[item.varHash] = item;

    //     ONDEMANDLOG(info) << "Subscribed: " << varName << " @ " << frequency << "ms";
    //     return true;
    // }

    // size_t OnDemandSub::batchSubscribe(const std::vector<SubscriptionItem> &items)
    // {
    //     size_t successCount = 0;

    //     std::unique_lock lock(subMutex_);
    //     for (const auto &item : items) {
    //         if (subscriptions_.find(item.varHash) == subscriptions_.end()) {
    //             subscriptions_[item.varHash] = item;
    //             successCount++;
    //         }
    //     }

    //     ONDEMANDLOG(info) << "Batch subscribed " << successCount << "/" << items.size() << " vars";
    //     return successCount;
    // }

    // bool OnDemandSub::unsubscribe(const std::string &varName)
    // {
    //     uint64_t varHash = fast_hash(varName);

    //     std::unique_lock lock(subMutex_);

    //     auto it = subscriptions_.find(varHash);
    //     if (it == subscriptions_.end()) {
    //         ONDEMANDLOG(WARNING) << "Not subscribed: " << varName;
    //         return false;
    //     }

    //     subscriptions_.erase(it);

    //     ONDEMANDLOG(info) << "Unsubscribed: " << varName;
    //     return true;
    // }

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
