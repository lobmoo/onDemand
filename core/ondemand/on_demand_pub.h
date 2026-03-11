/**
 * @file on_demand_pub.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2026-03-05
 * 
 * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2026-03-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
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

    class OnDemandPub : public DdsWrapper::ParticipantListener
    {
    public:
        // varName: 变量全名, newFreqMs: 新的发送周期(ms), 0xFFFFFFFF 表示无订阅者
        using FreqChangeCallback =
            std::function<void(const std::string &varName, uint32_t newFreqMs)>;

        OnDemandPub();
        ~OnDemandPub();

        /**
         * @brief 初始化发布者节点
         * @param  nodeName   节点名称，需全局唯一
         * @return true
         * @return false
         */
        bool init(const std::string &nodeName);

        /**
         * @brief 启动发布者节点，开始处理订阅请求和发布数据
         * @return true 
         * @return false 
         */
        bool start();

        /**
         * @brief 停止发布者节点，清理资源，停止所有定时器和线程
         */
        void stop();

        /**
         * @brief 创建变量并发布表定义，支持增量更新
         * @param  VarDefines       变量定义列表，包含变量名称、类型、频率等信息
         * @return true 
         * @return false 
         */
        bool createVars(const std::vector<DSF::Var::Define> &VarDefines);

        /**
         * @brief 删除变量并发布更新的表定义，支持增量更新
         * @param  varNames       变量名称列表，指定要删除的变量
         * @return true 
         * @return false 
         */
        bool deleteVars(const std::vector<std::string> &varNames);

        /**
         * @brief 设置变量数据，支持增量更新和分片发布
         * @param  varName          变量名称，必须已创建
         * @param  data             变量数据指针，指向外部数据源，发布时零拷贝
         * @param  size             数据大小，单位字节，支持分片发布
         * @return true 
         * @return false 
         */
        bool setVarData(const char *varName, const void *data, size_t size);

        /**
         * @brief 设置变量频率变化回调，当变量的发送周期发生改变时触发
         * @param  cb 回调函数，参数为变量全名和新的发送周期(ms)
         */
        void setFreqChangeCallback(FreqChangeCallback cb);

    private:
        // ParticipantListener 回调
        // void onReaderDiscovery(const DdsWrapper::EndpointInfo &info) override;
        /**
         * @brief 订阅者发现回调，处理新订阅者的注册信息，更新内部状态
         * @param  info             订阅者端点信息，包含节点名称、订阅的变量和频率等
         */
        void onWriterDiscovery(const DdsWrapper::EndpointInfo &info) override;

    private:
        /**
        * @brief 创建 DDS 读写器，设置回调函数
        * @return true 
        * @return false 
        */
        bool createTableDefineWriter();
        /**
         * @brief 创建数据传输写入器，按 bucket 分配，支持分片发布
         * @return true 
         * @return false 
         */
        bool createDataTransferWriter();
        /**
         * @brief 创建订阅注册读取器，设置回调函数处理订阅者注册请求
         * @param  processFunc      
         * @return true 
         * @return false 
         */
        bool createSubTableRegisterReader(
            std::function<void(const std::string &,
                               std::shared_ptr<DSF::Message::SubTableRegister>)>
                processFunc);

        /**
         * @brief 发布表定义，包含变量定义和频率信息，支持增量更新
         * @param  pubTableDefine 、发布的表定义信息，包含变量列表和频率等元数据
         * @return true 
         * @return false 
         */
        bool tableDefinePublish(const DSF::Var::PubTableDefine &pubTableDefine);

        /**
         * @brief 处理订阅者注册请求，更新订阅者信息和变量频率，触发发布调度
         * @param  topicName       订阅者注册的 DDS 主题名称
         * @param  data           数据
         * @return true 
         * @return false 
         */
        bool onReceiveRegisterCb(const std::string &topicName,
                                 std::shared_ptr<DSF::Message::SubTableRegister> data);
        /**
         * @brief 处理订阅者注册请求
         */
        void processReceiveRegister();

        /**
         * @brief 处理订阅者注册请求，更新订阅者信息和变量频率，触发发布调度
         * @param  nodeName        变量名
         * @param  varFreqs       周期
         */
        void handleSubscribe(const std::string &nodeName,
                             const std::vector<DSF::NamedValue> &varFreqs);
        /**
         * @brief 处理订阅者取消订阅请求，更新订阅者信息和变量频率，触发发布调度
         * @param  nodeName         变量名
         * @param  varFreqs         周期
         */
        void handleUnsubscribe(const std::string &nodeName,
                               const std::vector<DSF::NamedValue> &varFreqs);

        /**
         * @brief  获取或分配订阅者节点的位图位置，支持最多 256 个订阅者
         * @param  nodeHash        订阅者节点名称的 hash 值
         * @return uint8_t 
         */
        uint8_t getOrAssignNodeBit(uint64_t nodeHash);
        void recalcCurrentFreq(VarMetadata &meta);

        /**
         * @brief 发布分组键 = (bucketIndex, freqMs)
         * 
         * 同一 bucket (DDS topic) 内、同一频率的变量归为一组，
         * 共享一个时间轮定时器，一次触发批量发布组内所有变量。
         */
        struct PublishGroupKey {
            uint32_t bucketIndex;
            uint32_t freqMs;
            bool operator==(const PublishGroupKey &o) const
            {
                return bucketIndex == o.bucketIndex && freqMs == o.freqMs;
            }
        };

        struct PublishGroupKeyHash {
            size_t operator()(const PublishGroupKey &k) const
            {
                uint64_t combined =
                    (static_cast<uint64_t>(k.bucketIndex) << 32) | static_cast<uint64_t>(k.freqMs);
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

        /**
         * @brief 调度器回调，发布指定分组的数据
         */
        void processPublishTaskScheduler();

        /**
        * @brief 发布分组数据，增量 diff 方式，仅发布变更的变量，支持分片发布
        * @param  bucketIndex      bucket 索引，确定 DDS 主题
        * @param  freqMs           发布频率，单位毫秒，确定分组和调度周期
        */
        void publishGroupData(uint32_t bucketIndex, uint32_t freqMs);

        /**
        * @brief 取消指定分组的发布定时器，通常在分组成员变更或频率变更时调用
        */
        void cancelAllPublishTimers();

        /**
         * @brief 频率变化回调派发线程，从队列消费并调用用户回调，与注册处理线程解耦
         */
        void processFreqChangeCallback();

    private:
        /*变量索引: hash -> 元数据*/
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
        moodycamel::ConcurrentQueue<std::pair<std::string, uint32_t>> freqChangeQueue_;

        // ---- DDS 读写器 ----
        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Var::PubTableDefine>> pubTableDefineWriter_;
        std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Message::SubTableRegister>>
            subTableRegisterReqReader_;
        std::mutex DataTransferWriterMapMutex_;
        std::unordered_map<uint32_t,
                           std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Var::TableDataTransfer>>>
            dataTransferWriterMap_;

        VarStore varStore_;

        /*订阅者相关信息 */
        std::unordered_map<uint64_t, uint8_t> nodeSlotMap_;
        uint8_t nextNodeSlot_ = 0;

        FreqChangeCallback freqChangeCb_;

        /*注册请求处理线程 */
        std::thread registerProcessThread_;
        std::thread freqChangeCbThread_;

        /*时间轮调度器 */
        std::unique_ptr<TimerScheduler> publishScheduler_;
        std::mutex publishGroupsMutex_;
        std::unordered_map<PublishGroupKey, std::shared_ptr<TimerEventInterface>,
                           PublishGroupKeyHash>
            publishGroupTimers_;
        std::unordered_map<PublishGroupKey, std::shared_ptr<std::vector<GroupVarInfo>>,
                           PublishGroupKeyHash>
            groupMembers_;
        std::thread publishSchedulerThread_;
        std::atomic<bool> schedulerDirty_{true}; // varIndex_ 变更标记
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_PUB_V2_H
