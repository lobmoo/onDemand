/**
 * @file on_demand_sub.h
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

#ifndef ON_DEMAND_SUB_H
#define ON_DEMAND_SUB_H

#include "on_demand_common.h"
#include "on_demand_listener.h"
#include "concurrentqueue.h"
#include "variable_store.h"
#include "timer_wheel/timer_scheduler.h"
#include <shared_mutex>
#include <cstdint>
#include <string_view>
#include <sys/types.h>
#include <set>
#include <shared_mutex>

namespace dsf
{
namespace ondemand
{
    struct SubscriptionItem {
        std::string varName; // 变量名称
        uint32_t frequency;  // 订阅频率，单位ms

        SubscriptionItem(const std::string &var, uint32_t freq) : varName(var), frequency(freq) {}
    };

    /**
     * @brief 单个变量的回调数据
     */
    struct VarCallbackData {
        std::string_view nodeName;
        std::string_view varName;
        std::string_view varType;
        std::string_view type_version;
        const void *data;
        size_t size;
        uint64_t timestampNs;
        DSF::Var::BLOB_TYPE blobType{DSF::Var::BLOB_TYPE::UNKNOWN};
    };

    /**
     * @brief 批量数据回调函数 (同频同源的一批变量一次性回调)
     */
    using DataCallback = std::function<void(const std::vector<VarCallbackData> &vars)>;

    /**
     * @brief TableDefine 回调，收到 pub 端广播的变量定义时触发
     */
    using TableDefineCallback = std::function<void(const std::vector<DSF::Var::Define> &defines)>;

    /**
 * @brief 按需订阅器 V2 - 重构版本
 */
    class OnDemandSub : public OnDemandListener
    {
    public:
        OnDemandSub();
        virtual ~OnDemandSub();

        /**
        * @brief 初始化订阅器
        */
        bool init(const std::string &nodeName);

        /**
        * @brief 启动订阅器
        */
        bool start();

        /**
        * @brief 停止订阅器
        */
        void stop();

        /**
        * @brief 订阅变量并注册回调
        * @param  node_name 节点名        
        * @param  items      变量信息列表     
        * @param  callback   数据回调函数，在时间轮定时器触发时被调用
        * @return true 成功 / false 失败 
        */
        bool subscribe(const char *node_name, const std::vector<SubscriptionItem> &items,
                       DataCallback callback = nullptr);

        /**
         * @brief 获取总接收数量
         * @return uint64_t 
         */
        uint64_t getTotalReceivedVars() const { return totalReceived_.load(); }

        /**
         * @brief 获取当前已收到定义的所有可订阅变量，按节点名分组
         * @return nodeName -> varName 列表
         */
        std::unordered_map<std::string, std::vector<std::string>> getAvailableVars() const;

        /**
        * @brief 取消订阅
        */
        bool unsubscribe(const char *node_name, const std::vector<std::string> &items);


        /**
         * @brief 注册 TableDefine 回调，收到 pub 端变量定义时触发
         * @param  cb  外部可能需要提前得知对端发送的变量定义，此时应当提前拿到数据定义回调并注册，以免错过第一次的定义广播
         */
        void setTableDefineCallback(TableDefineCallback cb)
        {
            std::lock_guard<std::mutex> lock(tableDefineCbMutex_);
            tableDefineCb_ = std::move(cb);
        }

        /**
         * @brief 同步读取变量当前值，填充 VarCallbackData
         * @param node_name  发布节点名
         * @param var_name   变量名
         * @param out        输出结果。注意：VarCallbackData 中的所有 string_view 均为非拥有视图，调用方如需跨越其有效期保存，
         *                   必须立即复制为 std::string。
         *                   - out.data 指向线程局部缓冲区，在同线程下次调用前有效。
         *                   - out.nodeName / out.varName / out.varType / out.type_version：
         *                     若变量定义已收到，这些 string_view 指向订阅器内部 Define 对象中的存储，
         *                     在对应变量定义保持注册期间有效。
         *                   - 若变量定义尚未收到，out.nodeName / out.varName 指向线程局部 std::string 缓冲区（由
         *                     订阅器内部持有），在同线程下次调用 varReadSync 前有效；out.varType / out.type_version
         *                     为空 string_view。调用方无需关心入参 node_name / var_name 的生命期。
         * @return true 成功 / false 变量不存在或尚未收到数据
         */
        bool varReadSync(const char *node_name, const char *var_name, VarCallbackData &out) const;

        /**
         * @brief 手动清理指定 pub 节点的所有变量（用于外部检测到 pub 掉线的场景）
         * @param  pubNodeName  pub 节点名称
         * @return true  找到并清理成功
         * @return false 未找到该节点或参数无效
         */
        bool cleanupParticipantPublish(const std::string &pubNodeName);

        DSF::Var::BLOB_TYPE getBlobType() const
        {
            return static_cast<DSF::Var::BLOB_TYPE>(blobType_.load(std::memory_order_acquire));
        }

    private:
        /**
         * @brief Participant 掉线回调，清除该 publisher 的所有变量定义
         * @param  info  participant 信息，包含节点名称和状态
         */
        void onParticipantDiscovery(const DdsWrapper::ParticipantInfo &info) override;

    private:
        /**
       * @brief 创建变量定义数据读取器
       * @param  processFunc      
       * @return true 
       * @return false 
       */
        bool createTableDefineReader(
            std::function<void(const std::string &, std::shared_ptr<DSF::Var::PubTableDefine>)>
                processFunc);
        /**
          * @brief 创建频率请求数据写入器
          * @return true 
          * @return false 
          */
        bool createSubTableRegisterWriter();

        /**
         * @brief 创建数据传输读取器
         * @param  processFuncstd   
         * @return true 
         * @return false 
         */
        bool createDataTransferReader(
            std::function<void(const std::string &, std::shared_ptr<DSF::Var::TableDataTransfer>)>
                processFunc,
            const std::unordered_set<uint32_t> *targetBucketIds = nullptr);

        /**
         * @brief 处理变量定义数据回调函数
         * @param  topicName        
         * @param  data             
         */
        void onReceiveTableDefineCb(const std::string &topicName,
                                    std::shared_ptr<DSF::Var::PubTableDefine> data);
        /**
        * @brief 
        * @param  topicName        MyParamDoc
        * @param  data             MyParamDoc
        */
        void onReceiveDataTransferCb(const std::string &topicName,
                                     std::shared_ptr<DSF::Var::TableDataTransfer> data);

        /**
        * @brief 处理变量定义数据
        */
        void processTableDefine();

        /**
        * @brief 处理接收数据
        */
        void processDataTransfer();

        /**
         * @brief Set the Partion 
         * @param  name           subscriber name
         * @param  partitionName    partition name
         * @return true 
         * @return false 
         */
        bool setPartition(std::string name, std::string partitionName);
        /**
         * @brief 回调分组键 = (bucketIndex, freqMs)，与 pub 端 PublishGroupKey 对齐
         *
         * 同一 bucket、同一频率的变量归为一组，共享一个时间轮定时器，
         * jitter 与 pub 一致，确保 sub 回调在 pub 发完该 bucket 后触发。
         */
        struct CallbackGroupKey {
            uint32_t bucketIndex;
            uint32_t freqMs;
            bool operator==(const CallbackGroupKey &o) const
            {
                return bucketIndex == o.bucketIndex && freqMs == o.freqMs;
            }
        };

        struct CallbackGroupKeyHash {
            size_t operator()(const CallbackGroupKey &k) const
            {
                uint64_t combined =
                    (static_cast<uint64_t>(k.bucketIndex) << 32) | static_cast<uint64_t>(k.freqMs);
                return std::hash<uint64_t>{}(combined);
            }
        };

        /**
         * @brief 回调分组成员信息
         */
        struct CallbackVarInfo {
            uint64_t varHash;
            int32_t varId;
            uint32_t dataSize;
            uint32_t bucketIndex;
            std::string nodeName;
            std::string varName;
            std::string varType;
            std::string typeVersion;
        };

        /**
         * @brief 订阅回调信息 (用户调用 subscribe 时存储)
         */
        struct SubCallbackInfo {
            uint32_t freqMs;
            DataCallback callback;
            std::string varName;
        };

        /**
         * @brief 回调调度器主循环，扫描订阅回调信息，构建分组并管理定时器
         */
        void processCallbackScheduler();

        /**
         * @brief 回调分组数据，读取 VarStore 并调用用户回调
         * @param  bucketIndex bucket 索引
         * @param  freqMs 回调频率，单位毫秒
         */
        void callbackGroupData(uint32_t bucketIndex, uint32_t freqMs);

        /**
         * @brief 取消所有回调定时器并清空分组
         */
        void cancelAllCallbackTimers();

    private:
        std::string nodeName_;
        TableDefineCallback tableDefineCb_;
        std::mutex tableDefineCbMutex_; // 保护 tableDefineCb_
        std::shared_ptr<DdsWrapper::DataNode> dataNode_;

        /*通信writer/reader*/
        std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Var::PubTableDefine>>
            pubTableDefineReader_; // 变量定义数据读取器
        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Message::SubTableRegister>>
            subTableRegisterReqWriter_;
        std::mutex dataTransferCtxMapMutex_; // 互斥锁
        std::unordered_map<uint32_t,
                           std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Var::TableDataTransfer>>>
            dataTransferReaderMap_; // 接收数据读取器map，key为bucket id
        std::set<std::string> activePartitions_; // 已激活的 DDS partition 集合
        std::mutex activePartitionsMutex_;       // 保护 activePartitions_

        /*线程相关*/
        std::atomic<bool> initialized_;
        std::atomic<bool> running_;
        std::atomic<uint64_t> totalReceived_;

        /*变量定义队列*/
        moodycamel::ConcurrentQueue<std::shared_ptr<DSF::Var::PubTableDefine>> pubTableDefineQueue_;
        moodycamel::ConcurrentQueue<std::shared_ptr<DSF::Var::TableDataTransfer>>
            dataTransferQueue_;

        /*处理线程*/
        std::thread processTableDefineThread_;
        std::vector<std::thread> processDataTransferThreads_;
         mutable std::shared_mutex varIndexMutex_;
        /*变量索引: hash -> 元数据（热路径）*/
        std::unordered_map<uint64_t, VarMetadata> varIndex_;
        /*变量定义冷路径索引: hash -> IDL Define（仅广播/查询时访问）
         * 与 varIndex_ 共享 varIndexMutex_，生命周期与 varIndex_ 条目一致 */
        std::unordered_map<uint64_t, std::shared_ptr<DSF::Var::Define>> varDefineIndex_;
       
        VarStore varStore_; // 变量值存储

        /*写入时间戳/计数，按 bucket 粒度跟踪 (lock-free, 单写多读)*/
        struct VarWriteStamp {
            std::atomic<uint64_t> timestampNs{0};
            std::atomic<uint32_t> writeCount{0};
            std::atomic<uint32_t> blobType{static_cast<uint32_t>(DSF::Var::BLOB_TYPE::UNKNOWN)};
        };
        VarWriteStamp varWriteStamps_[ONDEMAND_BUCKET_SIZE];

        /*订阅回调存储: varHash -> 回调信息*/
        std::unordered_map<uint64_t, SubCallbackInfo> subscriptionCallbacks_;
        std::mutex subscriptionCallbacksMutex_;

        /*时间轮回调调度器*/
        std::unique_ptr<TimerScheduler> callbackScheduler_;
        std::mutex callbackGroupsMutex_;
        std::unordered_map<CallbackGroupKey, std::shared_ptr<TimerEventInterface>,
                           CallbackGroupKeyHash>
            callbackGroupTimers_;
        struct CallbackGroupEntry {
            std::shared_ptr<std::vector<CallbackVarInfo>> members;
            std::shared_ptr<std::atomic<bool>> running{std::make_shared<std::atomic<bool>>(false)};
            DataCallback callback; // 组级回调，不再从 members 逐个查找
        };
        std::unordered_map<CallbackGroupKey, CallbackGroupEntry, CallbackGroupKeyHash>
            callbackGroupMembers_;
        std::thread callbackSchedulerThread_;
        std::atomic<bool> callbackDirty_{false};

        std::atomic<uint32_t> blobType_{static_cast<uint32_t>(DSF::Var::BLOB_TYPE::STRUCTS)};

        /* participant GUID 跟踪：pubName -> guidPrefix*/
        std::mutex pubGuidsMutex_;
        std::unordered_map<std::string, std::string> pubGuids_;
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_SUB_V2_H
