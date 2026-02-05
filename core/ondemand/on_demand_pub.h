/**
 * @file on_demand_pub_v2.h
 * @brief 按需发布系统 - 重构版本
 * @date 2026-02-04
 * @author DSF Team
 * 
 * 架构特点:
 * - 使用 uint64_t hash 替代 string，减少内存占用
 * - 时间轮调度器，高效频率管理
 * - Zero-Copy 环形缓冲区
 * - 无锁队列设计
 * - 批量操作优化
 */

#ifndef ON_DEMAND_PUB_V2_H
#define ON_DEMAND_PUB_V2_H

#include <shared_mutex>
#include <memory>
#include "on_demand_common.h"


namespace dsf {
namespace ondemand {

/**
 * @brief 变量元数据 ()
 */
struct VarMetadata {
    uint64_t varHash;           // 变量名 hash (唯一标识)
    uint64_t bucketHash;        // 所属桶 hash
    uint16_t varId;             // 变量ID (桶内唯一)
    uint16_t bucketIdx;         // 桶索引 (0-255)
    uint32_t currentFreq;       // 当前发布频率 (ms)  
    // 订阅频率信息 (紧凑存储)
    struct FreqSub {
        uint32_t freq;          // 订阅频率
        uint16_t subCount;      // 订阅数量
        uint64_t subMask;       // 订阅者掩码 (最多64个节点)   
        FreqSub() : freq(0), subCount(0), subMask(0) {}
    };
    
    std::array<FreqSub, 20> freqSubs;  // 最多20个不同频率
    uint8_t activeFreqCount;          // 活跃频率数量
    
    VarMetadata() 
        : varHash(0), bucketHash(0), varId(0), bucketIdx(0), 
          currentFreq(0xFFFFFFFF), activeFreqCount(0) {}
};

/**
 * @brief 变量定义 (用于创建)
 */
struct VarDef {
    std::string name;           // 变量名
    std::string tableName;      // 表名
    uint32_t defaultFreq;       // 默认频率
    
    VarDef(const std::string& n, const std::string& t, uint32_t f = 0xFFFFFFFF)
        : name(n), tableName(t), defaultFreq(f) {}
};


class OnDemandPub{
public:
    OnDemandPub();
    ~OnDemandPub();

    /**
     * @brief 初始化发布器
     * @param nodeName 节点名称
     * @param dds DDS抽象层
     */
    bool init(const std::string& nodeName);
    
    /**
     * @brief 启动发布器
     */
    bool start();
    
    /**
     * @brief 停止发布器
     */
    void stop();
    
    /**
     * @brief 创建变量
     * @param varName 变量名
     * @param tableName 表名
     * @return 成功返回 true
     */
    bool createVar(const char* varName, const char* tableName);
    
    /**
     * @brief 批量创建变量
     * @param defs 变量定义列表
     * @return 成功数量
     */
    size_t batchCreateVars(const std::vector<VarDef>& defs);
    
    /**
     * @brief 设置变量数据
     * @param varName 变量名
     * @param data 数据指针
     * @param size 数据大小
     */
    bool setVarData(const char* varName, const void* data, size_t size);
    
    /**
     * @brief 处理订阅请求
     * @param nodeName 节点名称
     * @param vars 变量列表 <变量名, 频率>
     */
    void handleSubscribe(const std::string& nodeName,
                        const std::vector<std::pair<std::string, uint32_t>>& vars);
    
    /**
     * @brief 导出状态
     */
    void dumpState(std::ostream& os);
    
    /**
     * @brief 获取内存使用 (字节)
     */
    size_t getMemoryUsage() const;
    
private:
    // 变量索引: hash -> 元数据
    std::unordered_map<uint64_t, VarMetadata> varIndex_;
    mutable std::shared_mutex varIndexMutex_;
    
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;

    /*交互dds节点*/
    std::shared_ptr<DdsWrapper::DataNode> dataNode_;
    std::string nodeName_;
};

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_PUB_V2_H
