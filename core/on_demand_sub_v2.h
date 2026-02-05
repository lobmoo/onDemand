/**
 * @file on_demand_sub_v2.h
 * @brief 按需订阅系统 - 重构版本
 * @date 2026-02-04
 */

#ifndef ON_DEMAND_SUB_V2_H
#define ON_DEMAND_SUB_V2_H

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <functional>
#include "on_demand_common.h"
#include "dds_wrapper/dds_abstraction.h"

namespace dsf {
namespace ondemand {
namespace v2 {

/**
 * @brief 订阅项
 */
struct SubscriptionItem {
    std::string varName;
    std::string tableName;
    uint64_t varHash;
    uint32_t frequency;
    
    SubscriptionItem(const std::string& vn, const std::string& tn, uint32_t freq)
        : varName(vn), tableName(tn), varHash(fast_hash(vn)), frequency(freq) {}
};

/**
 * @brief 数据回调函数
 */
using DataCallback = std::function<void(const std::string& varName, 
                                       const void* data, 
                                       size_t size)>;

/**
 * @brief 按需订阅器 V2 - 重构版本
 */
class OnDemandSubscriberV2 {
public:
    OnDemandSubscriberV2();
    ~OnDemandSubscriberV2();
    
    /**
     * @brief 初始化订阅器
     */
    bool init(const std::string& nodeName, std::shared_ptr<DDSAbstraction> dds);
    
    /**
     * @brief 启动订阅器
     */
    bool start();
    
    /**
     * @brief 停止订阅器
     */
    void stop();
    
    /**
     * @brief 订阅变量
     */
    bool subscribe(const std::string& varName, 
                   const std::string& tableName, 
                   uint32_t frequency);
    
    /**
     * @brief 批量订阅变量
     */
    size_t batchSubscribe(const std::vector<SubscriptionItem>& items);
    
    /**
     * @brief 取消订阅
     */
    bool unsubscribe(const std::string& varName);
    
    /**
     * @brief 设置数据回调函数
     */
    void setDataCallback(DataCallback callback) {
        dataCallback_ = callback;
    }
    
    /**
     * @brief 获取订阅数量
     */
    size_t getSubscriptionCount() const;
    
    /**
     * @brief 导出状态
     */
    void dumpState(std::ostream& os);
    
private:
    std::unordered_map<uint64_t, SubscriptionItem> subscriptions_;
    mutable std::shared_mutex subMutex_;
    
    DataCallback dataCallback_;
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    std::string nodeName_;
    std::atomic<uint64_t> totalReceived_;
};

} // namespace v2
} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_SUB_V2_H
