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

#ifndef ON_DEMAND_PUB_H
#define ON_DEMAND_PUB_H

#include <shared_mutex>
#include <memory>
#include "on_demand_common.h"

namespace dsf
{
namespace ondemand
{

    /**
 * @brief 变量元数据 ()
 */
    struct VarMetadata {
        uint64_t varHash;                            // 变量名 hash (唯一标识)
        BucketManager::BucketIndex bucketIndex;      // 所属桶索引 (0-19)
        uint16_t varId;                              // 变量ID (桶内唯一)
        uint32_t currentFreq;                        // 当前发布频率 (ms)
        std::shared_ptr<DSF::Var::Define> varDefine; // 变量定义 (包含结构信息等)

        // 订阅频率信息 (紧凑存储)
        struct FreqSub {
            uint32_t freq;     // 订阅频率
            uint16_t subCount; // 订阅数量
            uint64_t subMask;  // 订阅者掩码 (最多64个节点)
            FreqSub() : freq(0), subCount(0), subMask(0) {}
        };

        std::vector<FreqSub> freqSubs; // 按需分配
        uint8_t activeFreqCount;       // 活跃频率数量

        VarMetadata()
            : varHash(0), bucketIndex(0), varId(0), currentFreq(0xFFFFFFFF), freqSubs{},
              activeFreqCount(0)
        {
        }
    };

    class OnDemandPub
    {
    public:
        OnDemandPub();
        ~OnDemandPub();

        /**
         * @brief 
         * @param  nodeName  节点名称
         * @return true 
         * @return false 
         */
        bool init(const std::string &nodeName);

        /**
        * @brief 启动发布器
        * @return true 
        * @return false 
        */
        bool start();

        /**
        * @brief 停止发布器
        */
        void stop();

        /**
         * @brief 暂停发布器
         */
        void pause();

        /**
          * @brief 恢复发布器
          */
        void resume();

        /**
         * @brief 批量创建变量
         * @param  VarDefines     变量定义数组  
         * @return size_t 
         */
        bool createVars(const std::vector<DSF::Var::Define> &VarDefines);

        /**
         * @brief 批量删除变量
         * @param  varNames   变量名称数组
         * @return true 
         * @return false 
         */
        bool deleteVars(const std::vector<std::string> &varNames);

        /**
         * @brief Set the Var Data object
         * @param  varName          MyParamDoc
         * @param  data             MyParamDoc
         * @param  size             MyParamDoc
         * @return true 
         * @return false 
         */
        bool setVarData(const char *varName, const void *data, size_t size);

        /**
     * @brief 处理订阅请求
     * @param nodeName 节点名称
     * @param vars 变量列表 <变量名, 频率>
     */
        void handleSubscribe(const std::string &nodeName,
                             const std::vector<std::pair<std::string, uint32_t>> &vars);

        /**
     * @brief 导出状态
     */
        void dumpState(std::ostream &os);

        /**
     * @brief 获取内存使用 (字节)
     */
        size_t getMemoryUsage() const;

    private:
        // 辅助函数
        bool createTableDefineWriter();
        bool createSubTableRegisterReader();
        bool tableDefinePublish(const DSF::Var::PubTableDefine &pubTableDefine);

        // 变量索引: hash -> 元数据
        std::unordered_map<uint64_t, VarMetadata> varIndex_;
        mutable std::shared_mutex varIndexMutex_;

        BucketManager bucketManager_;

        std::atomic<bool> initialized_;
        std::atomic<bool> running_;

        /*交互dds节点*/
        std::shared_ptr<DdsWrapper::DataNode> dataNode_;
        std::string nodeName_;

        /*数据读写器*/
        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Var::PubTableDefine>>
            pubTableDefineWriter_; // 变量定义数据写入器
        std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Message::SubTableRegister>>
            subTableRegisterReqReader_; // 频率请求数据读取器
        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Message::SubTableRegister>>
            subTableRegisterRespWriter_; // 频率回复数据写入器
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_PUB_V2_H
