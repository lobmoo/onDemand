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

    OnDemandPub::OnDemandPub() : initialized_(false), running_(false) {}

    OnDemandPub::~OnDemandPub() { stop(); }

    bool OnDemandPub::init(const std::string &nodeName)
    {
        if (initialized_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already initialized";
            return false;
        }

        nodeName_ = nodeName;

        /*创建节点*/


        ONDEMANDLOG(info) << "OnDemandPub initialized: " << nodeName;
        return true;
    }

    bool OnDemandPub::start()
    {
        if (running_.exchange(true)) {
            ONDEMANDLOG(warning) << "Already running";
            return false;
        }

        ONDEMANDLOG(info) << "OnDemandPub started";
        return true;
    }

    void OnDemandPub::stop()
    {
        if (!running_.exchange(false)) {
            return;
        }

        ONDEMANDLOG(info) << "OnDemandPub stopped";
    }

    bool OnDemandPub::createVar(const char *varName, const char *tableName)
    {
        uint64_t varHash = fast_hash(varName);

        std::unique_lock lock(varIndexMutex_);

        if (varIndex_.find(varHash) != varIndex_.end()) {
            ONDEMANDLOG(warning) << "Variable already exists: " << varName;
            return false;
        }

        VarMetadata meta;
        meta.varHash = varHash;
        meta.currentFreq = 0xFFFFFFFF;
        meta.activeFreqCount = 0;

        varIndex_[varHash] = meta;

        ONDEMANDLOG(debug) << "Created var: " << varName;
        return true;
    }

    size_t OnDemandPub::batchCreateVars(const std::vector<VarDef> &defs)
    {
        size_t successCount = 0;

        std::unique_lock lock(varIndexMutex_);
        for (const auto &def : defs) {
            uint64_t varHash = fast_hash(def.name);

            if (varIndex_.find(varHash) == varIndex_.end()) {
                VarMetadata meta;
                meta.varHash = varHash;
                meta.currentFreq = def.defaultFreq;
                meta.activeFreqCount = 0;

                varIndex_[varHash] = meta;
                successCount++;
            }
        }

        ONDEMANDLOG(info) << "Batch created " << successCount << "/" << defs.size() << " vars";
        return successCount;
    }

    bool OnDemandPub::setVarData(const char *varName, const void *data, size_t size)
    {
        uint64_t varHash = fast_hash(varName);

        std::shared_lock lock(varIndexMutex_);
        if (varIndex_.find(varHash) == varIndex_.end()) {
            ONDEMANDLOG(warning) << "Variable not found: " << varName;
            return false;
        }

        // TODO: 写入数据缓存
        return true;
    }

    void
    OnDemandPub::handleSubscribe(const std::string &nodeName,
                                         const std::vector<std::pair<std::string, uint32_t>> &vars)
    {
        uint64_t nodeHash = fast_hash(nodeName);

        std::unique_lock lock(varIndexMutex_);
        for (const auto &[varName, freq] : vars) {
            uint64_t varHash = fast_hash(varName);

            auto it = varIndex_.find(varHash);
            if (it == varIndex_.end()) {
                continue;
            }

            auto &meta = it->second;

            // 更新频率
            if (freq < meta.currentFreq) {
                meta.currentFreq = freq;
            }
        }

        ONDEMANDLOG(info) << "Node " << nodeName << " subscribed to " << vars.size() << " vars";
    }

    void OnDemandPub::dumpState(std::ostream &os)
    {
        os << "========== OnDemandPub State ==========\n";
        os << "Node: " << nodeName_ << "\n";
        os << "Running: " << (running_ ? "Yes" : "No") << "\n";

        std::shared_lock lock(varIndexMutex_);
        os << "Total Variables: " << varIndex_.size() << "\n";

        os << "==============================================\n";
    }

    size_t OnDemandPub::getMemoryUsage() const
    {
        std::shared_lock lock(varIndexMutex_);
        size_t total = varIndex_.size() * sizeof(VarMetadata);
        total += varIndex_.size() * (sizeof(uint64_t) + sizeof(void *));
        return total;
    }

} // namespace ondemand
} // namespace dsf
