// /**
//  * @file on_demand_pub_v2.cpp
//  * @brief 
//  * @author wwk (1162431386@qq.com)
//  * @version 1.0
//  * @date 2026-02-05
//  * 
//  * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
//  * 
//  * @par 修改日志:
//  * <table>
//  * <tr><th>Date       <th>Version <th>Author  <th>Description
//  * <tr><td>2026-02-05     <td>1.0     <td>wwk   <td>修改?
//  * </table>
//  */

// #include "on_demand_pub.h"


// namespace dsf
// {
// namespace ondemand
// {

//     OnDemandPublisherV2::OnDemandPublisherV2() : initialized_(false), running_(false) {}

//     OnDemandPublisherV2::~OnDemandPublisherV2() { stop(); }

//     bool OnDemandPublisherV2::init(const std::string &nodeName, std::shared_ptr<DDSAbstraction> dds)
//     {
//         if (initialized_.exchange(true)) {
//             ONDEMANDLOG(WARNING) << "Already initialized";
//             return false;
//         }

//         nodeName_ = nodeName;
//         ONDEMANDLOG(INFO) << "OnDemandPublisherV2 initialized: " << nodeName;
//         return true;
//     }

//     bool OnDemandPublisherV2::start()
//     {
//         if (running_.exchange(true)) {
//             ONDEMANDLOG(WARNING) << "Already running";
//             return false;
//         }

//         ONDEMANDLOG(INFO) << "OnDemandPublisherV2 started";
//         return true;
//     }

//     void OnDemandPublisherV2::stop()
//     {
//         if (!running_.exchange(false)) {
//             return;
//         }

//         ONDEMANDLOG(INFO) << "OnDemandPublisherV2 stopped";
//     }

//     bool OnDemandPublisherV2::createVar(const char *varName, const char *tableName)
//     {
//         uint64_t varHash = fast_hash(varName);

//         std::unique_lock lock(varIndexMutex_);

//         if (varIndex_.find(varHash) != varIndex_.end()) {
//             ONDEMANDLOG(WARNING) << "Variable already exists: " << varName;
//             return false;
//         }

//         VarMetadata meta;
//         meta.varHash = varHash;
//         meta.currentFreq = 0xFFFFFFFF;
//         meta.activeFreqCount = 0;

//         varIndex_[varHash] = meta;

//         ONDEMANDLOG(DEBUG) << "Created var: " << varName;
//         return true;
//     }

//     size_t OnDemandPublisherV2::batchCreateVars(const std::vector<VarDef> &defs)
//     {
//         size_t successCount = 0;

//         std::unique_lock lock(varIndexMutex_);
//         for (const auto &def : defs) {
//             uint64_t varHash = fast_hash(def.name);

//             if (varIndex_.find(varHash) == varIndex_.end()) {
//                 VarMetadata meta;
//                 meta.varHash = varHash;
//                 meta.currentFreq = def.defaultFreq;
//                 meta.activeFreqCount = 0;

//                 varIndex_[varHash] = meta;
//                 successCount++;
//             }
//         }

//         ONDEMANDLOG(INFO) << "Batch created " << successCount << "/" << defs.size() << " vars";
//         return successCount;
//     }

//     bool OnDemandPublisherV2::setVarData(const char *varName, const void *data, size_t size)
//     {
//         uint64_t varHash = fast_hash(varName);

//         std::shared_lock lock(varIndexMutex_);
//         if (varIndex_.find(varHash) == varIndex_.end()) {
//             ONDEMANDLOG(WARNING) << "Variable not found: " << varName;
//             return false;
//         }

//         // TODO: 写入数据缓存
//         return true;
//     }

//     void
//     OnDemandPublisherV2::handleSubscribe(const std::string &nodeName,
//                                          const std::vector<std::pair<std::string, uint32_t>> &vars)
//     {
//         uint64_t nodeHash = fast_hash(nodeName);

//         std::unique_lock lock(varIndexMutex_);
//         for (const auto &[varName, freq] : vars) {
//             uint64_t varHash = fast_hash(varName);

//             auto it = varIndex_.find(varHash);
//             if (it == varIndex_.end()) {
//                 continue;
//             }

//             auto &meta = it->second;

//             // 更新频率
//             if (freq < meta.currentFreq) {
//                 meta.currentFreq = freq;
//             }
//         }

//         ONDEMANDLOG(INFO) << "Node " << nodeName << " subscribed to " << vars.size() << " vars";
//     }

//     void OnDemandPublisherV2::dumpState(std::ostream &os)
//     {
//         os << "========== OnDemandPublisherV2 State ==========\n";
//         os << "Node: " << nodeName_ << "\n";
//         os << "Running: " << (running_ ? "Yes" : "No") << "\n";

//         std::shared_lock lock(varIndexMutex_);
//         os << "Total Variables: " << varIndex_.size() << "\n";

//         os << "==============================================\n";
//     }

//     size_t OnDemandPublisherV2::getMemoryUsage() const
//     {
//         std::shared_lock lock(varIndexMutex_);
//         size_t total = varIndex_.size() * sizeof(VarMetadata);
//         total += varIndex_.size() * (sizeof(uint64_t) + sizeof(void *));
//         return total;
//     }

// } // namespace ondemand
// } // namespace dsf
