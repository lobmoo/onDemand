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
#include "variable_store.h"

namespace dsf
{
namespace ondemand
{

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
        bool createSubTableRegisterReader(
            std::function<void(const std::string &,
                               std::shared_ptr<DSF::Message::SubTableRegister>)>
                processFunc);
        bool tableDefinePublish(const DSF::Var::PubTableDefine &pubTableDefine);

        /*内部函数 */
        bool onReceiveRegisterCb(const std::string &topicName,
                                 std::shared_ptr<DSF::Message::SubTableRegister> data);
        void processReceiveRegister();
        void handleSubscribe(const std::string &nodeName,
                             const std::vector<DSF::NamedValue> &varFreqs);
        void handleUnsubscribe(const std::string &nodeName,
                               const std::vector<DSF::NamedValue> &varFreqs);

        // 获取或分配节点的 bit 位索引 (用于 subMask)
        uint8_t getOrAssignNodeBit(uint64_t nodeHash);
        // 重新计算变量的最小发布频率
        void recalcCurrentFreq(VarMetadata &meta);

            private
            : std::unordered_map<uint64_t, VarMetadata> varIndex_; // 变量索引: hash -> 元数据
        mutable std::shared_mutex varIndexMutex_;

        BucketManager bucketManager_;

        std::atomic<bool> initialized_;
        std::atomic<bool> running_;

        std::shared_ptr<DdsWrapper::DataNode> dataNode_; //交互dds节点
        std::string nodeName_;                           //节点名称

        moodycamel::ConcurrentQueue<std::shared_ptr<DSF::Message::SubTableRegister>>
            pubTableDefRegisterQueue_; // 频率请求队列

        /*数据读写器*/
        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Var::PubTableDefine>>
            pubTableDefineWriter_; // 变量定义数据写入器
        std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Message::SubTableRegister>>
            subTableRegisterReqReader_; // 频率请求数据读取器
        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Message::SubTableRegister>>
            subTableRegisterRespWriter_; // 频率回复数据写入器

        VarStore varStore_; // 变量值存储

        // 节点 hash -> bit 位索引映射 (最多支持64个订阅节点)
        std::unordered_map<uint64_t, uint8_t> nodeSlotMap_;
        uint8_t nextNodeSlot_ = 0;

        std::thread registerProcessThread_; // 处理订阅请求的线程
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_PUB_V2_H
