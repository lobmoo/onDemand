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
 * - 按 (bucket, freq) 分组批量发布，增量 diff 驱动
 */

#ifndef ON_DEMAND_PUB_H
#define ON_DEMAND_PUB_H

#include <shared_mutex>
#include <memory>
#include "on_demand_common.h"
#include "variable_store.h"
#include "timer_wheel/timer_scheduler.h"

namespace dsf
{
namespace ondemand
{

    class OnDemandPub
    {
    public:
        OnDemandPub();
        ~OnDemandPub();

        bool init(const std::string &nodeName);
        bool start();
        void stop();

        bool createVars(const std::vector<DSF::Var::Define> &VarDefines);
        bool deleteVars(const std::vector<std::string> &varNames);
        bool setVarData(const char *varName, const void *data, size_t size);

    private:
        // ---- DDS topic 创建 ----
        bool createTableDefineWriter();
        bool createDataTransferWriter();
        bool createSubTableRegisterReader(
            std::function<void(const std::string &,
                               std::shared_ptr<DSF::Message::SubTableRegister>)>
                processFunc);
        bool tableDefinePublish(const DSF::Var::PubTableDefine &pubTableDefine);

        // ---- 订阅处理 ----
        bool onReceiveRegisterCb(const std::string &topicName,
                                 std::shared_ptr<DSF::Message::SubTableRegister> data);
        void processReceiveRegister();
        void handleSubscribe(const std::string &nodeName,
                             const std::vector<DSF::NamedValue> &varFreqs);
        void handleUnsubscribe(const std::string &nodeName,
                               const std::vector<DSF::NamedValue> &varFreqs);

        uint8_t getOrAssignNodeBit(uint64_t nodeHash);
        void recalcCurrentFreq(VarMetadata &meta);

        // ---- 分组发布调度 (增量 diff + 分片发送) ----

        /**
         * @brief 发布分组键 = (bucketIndex, freqMs)
         * 
         * 同一 bucket (DDS topic) 内、同一频率的变量归为一组，
         * 共享一个时间轮定时器，一次触发批量发布组内所有变量。
         */
        struct PublishGroupKey {
            uint32_t bucketIndex;
            uint32_t freqMs;
            bool operator==(const PublishGroupKey &o) const {
                return bucketIndex == o.bucketIndex && freqMs == o.freqMs;
            }
        };

        struct PublishGroupKeyHash {
            size_t operator()(const PublishGroupKey &k) const {
                uint64_t combined = (static_cast<uint64_t>(k.bucketIndex) << 32)
                                  | static_cast<uint64_t>(k.freqMs);
                return std::hash<uint64_t>{}(combined);
            }
        };

        /**
         * @brief 分组成员信息 (轻量，避免回调时查 varIndex_)
         */
        struct GroupVarInfo {
            uint64_t varHash;
            uint32_t varId;
            uint32_t dataSize;
        };

        // 调度主循环 (增量 diff 驱动)
        void processPublishTaskScheduler();
        // 批量发布组内数据 (定时器回调)
        void publishGroupData(uint32_t bucketIndex, uint32_t freqMs);
        // 取消所有发布定时器
        void cancelAllPublishTimers();
        // 构建并发送 TableDataTransfer (批量)
        bool sendBatchTableDataTransfer(
            uint32_t bucketIndex,
            const std::vector<std::pair<uint64_t, std::vector<uint8_t>>> &varDataList);

    private:
        // ---- 核心数据 ----
        std::unordered_map<uint64_t, VarMetadata> varIndex_;
        mutable std::shared_mutex varIndexMutex_;
        BucketManager bucketManager_;

        std::atomic<bool> initialized_;
        std::atomic<bool> running_;
        std::shared_ptr<DdsWrapper::DataNode> dataNode_;
        std::string nodeName_;

        // ---- 队列 ----
        moodycamel::ConcurrentQueue<std::shared_ptr<DSF::Message::SubTableRegister>>
            pubTableDefRegisterQueue_;

        // ---- DDS 读写器 ----
        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Var::PubTableDefine>>
            pubTableDefineWriter_;
        std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Message::SubTableRegister>>
            subTableRegisterReqReader_;
        std::mutex DataTransferWriterMapMutex_;
        std::unordered_map<uint32_t,
                           std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Var::TableDataTransfer>>>
            dataTransferWriterMap_;

        VarStore varStore_;

        // ---- 订阅者 slot ----
        std::unordered_map<uint64_t, uint8_t> nodeSlotMap_;
        uint8_t nextNodeSlot_ = 0;

        // ---- 线程 ----
        std::thread registerProcessThread_;

        // ---- 时间轮调度器 (增量 diff + 按组调度) ----
        std::unique_ptr<TimerScheduler> publishScheduler_;
        std::mutex publishGroupsMutex_;
        std::unordered_map<PublishGroupKey,
                           std::shared_ptr<TimerEventInterface>,
                           PublishGroupKeyHash>
            publishGroupTimers_;
        std::unordered_map<PublishGroupKey,
                           std::shared_ptr<std::vector<GroupVarInfo>>,
                           PublishGroupKeyHash>
            groupMembers_;
        std::thread publishSchedulerThread_;
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_PUB_V2_H
