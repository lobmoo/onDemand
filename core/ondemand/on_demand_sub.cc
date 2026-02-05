// /**
//  * @file on_demand_sub_v2.cpp
//  * @brief 按需订阅系统 - 重构版本实现
//  * @date 2026-02-04
//  */

// #include "on_demand_sub_v2.h"
// #include <algorithm>

// namespace dsf {
// namespace ondemand {
// namespace v2 {

// OnDemandSubscriberV2::OnDemandSubscriberV2()
//     : initialized_(false), running_(false), totalReceived_(0) {}

// OnDemandSubscriberV2::~OnDemandSubscriberV2() {
//     stop();
// }

// bool OnDemandSubscriberV2::init(const std::string& nodeName,
//                                 std::shared_ptr<DDSAbstraction> dds) {
//     if (initialized_.exchange(true)) {
//         ONDEMANDLOG(WARNING) << "Already initialized";
//         return false;
//     }
    
//     nodeName_ = nodeName;
//     ONDEMANDLOG(INFO) << "OnDemandSubscriberV2 initialized: " << nodeName;
//     return true;
// }

// bool OnDemandSubscriberV2::start() {
//     if (running_.exchange(true)) {
//         ONDEMANDLOG(WARNING) << "Already running";
//         return false;
//     }
    
//     ONDEMANDLOG(INFO) << "OnDemandSubscriberV2 started";
//     return true;
// }

// void OnDemandSubscriberV2::stop() {
//     if (!running_.exchange(false)) {
//         return;
//     }
    
//     ONDEMANDLOG(INFO) << "OnDemandSubscriberV2 stopped";
// }

// bool OnDemandSubscriberV2::subscribe(const std::string& varName,
//                                      const std::string& tableName,
//                                      uint32_t frequency) {
//     SubscriptionItem item(varName, tableName, frequency);
    
//     std::unique_lock lock(subMutex_);
    
//     if (subscriptions_.find(item.varHash) != subscriptions_.end()) {
//         ONDEMANDLOG(WARNING) << "Already subscribed: " << varName;
//         return false;
//     }
    
//     subscriptions_[item.varHash] = item;
    
//     ONDEMANDLOG(INFO) << "Subscribed: " << varName << " @ " << frequency << "ms";
//     return true;
// }

// size_t OnDemandSubscriberV2::batchSubscribe(const std::vector<SubscriptionItem>& items) {
//     size_t successCount = 0;
    
//     std::unique_lock lock(subMutex_);
//     for (const auto& item : items) {
//         if (subscriptions_.find(item.varHash) == subscriptions_.end()) {
//             subscriptions_[item.varHash] = item;
//             successCount++;
//         }
//     }
    
//     ONDEMANDLOG(INFO) << "Batch subscribed " << successCount << "/" << items.size() << " vars";
//     return successCount;
// }

// bool OnDemandSubscriberV2::unsubscribe(const std::string& varName) {
//     uint64_t varHash = fast_hash(varName);
    
//     std::unique_lock lock(subMutex_);
    
//     auto it = subscriptions_.find(varHash);
//     if (it == subscriptions_.end()) {
//         ONDEMANDLOG(WARNING) << "Not subscribed: " << varName;
//         return false;
//     }
    
//     subscriptions_.erase(it);
    
//     ONDEMANDLOG(INFO) << "Unsubscribed: " << varName;
//     return true;
// }

// size_t OnDemandSubscriberV2::getSubscriptionCount() const {
//     std::shared_lock lock(subMutex_);
//     return subscriptions_.size();
// }

// void OnDemandSubscriberV2::dumpState(std::ostream& os) {
//     os << "========== OnDemandSubscriberV2 State ==========\n";
//     os << "Node: " << nodeName_ << "\n";
//     os << "Running: " << (running_ ? "Yes" : "No") << "\n";
//     os << "Total Received: " << totalReceived_.load() << "\n";
    
//     std::shared_lock lock(subMutex_);
//     os << "Active Subscriptions: " << subscriptions_.size() << "\n\n";
    
//     os << "=== Subscriptions ===\n";
//     for (const auto& [hash, item] : subscriptions_) {
//         os << "  " << item.varName << " [" << item.tableName << "]"
//            << " @ " << item.frequency << "ms\n";
//     }
    
//     os << "==============================================\n";
// }

// } // namespace v2
// } // namespace ondemand
// } // namespace dsf
