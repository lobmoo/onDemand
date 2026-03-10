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
#include "concurrentqueue.h"
#include "variable_store.h"
#include "timer_wheel/timer_scheduler.h"
#include <bits/stdint-uintn.h>
#include <sys/types.h>
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
 * @brief 数据回调函数
 */
    using DataCallback =
        std::function<void(const std::string &varName, const void *data, size_t size, uint64_t timestampNs)>;

    /**
 * @brief 按需订阅器 V2 - 重构版本
 */
    class OnDemandSub : public DdsWrapper::ParticipantListener
    {
    public:
        OnDemandSub();
        ~OnDemandSub();

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
        * @brief 取消订阅
        */
        bool unsubscribe(const char *node_name, const std::vector<std::string> &items);

        /**
        * @brief 获取订阅数量
        */
        size_t getSubscriptionCount() const;

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
                processFunc);

        /**
         * @brief 处理变量定义数据回调函数
         * @param  topicName        
         * @param  data             
         * @return true 
         * @return false 
         */
        bool onReceiveTableDefineCb(const std::string &topicName,
                                    std::shared_ptr<DSF::Var::PubTableDefine> data);
        /**
        * @brief 
        * @param  topicName        MyParamDoc
        * @param  data             MyParamDoc
        * @return true 
        * @return false 
        */
        bool onReceiveDataTransferCb(const std::string &topicName,
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
         * @brief 回调分组键 = freqMs
         * 
         * 相同频率的所有回调变量归为一组，
         * 共享一个时间轮定时器，一次触发批量回调组内所有变量。
         */
        struct CallbackGroupKey {
            uint32_t freqMs;
            bool operator==(const CallbackGroupKey &o) const
            {
                return freqMs == o.freqMs;
            }
        };

        struct CallbackGroupKeyHash {
            size_t operator()(const CallbackGroupKey &k) const
            {
                return std::hash<uint32_t>{}(k.freqMs);
            }
        };

        /**
         * @brief 回调分组成员信息
         */
        struct CallbackVarInfo {
            uint64_t varHash;
            int32_t varId;
            uint32_t dataSize;
            std::string varName;
            DataCallback callback;
            uint32_t lastSeenWriteCount{0}; // 上次回调时的写入计数
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
         * @param  freqMs 回调频率，单位毫秒
         */
        void callbackGroupData(uint32_t freqMs);

        /**
         * @brief 取消所有回调定时器并清空分组
         */
        void cancelAllCallbackTimers();

    private:
        std::string nodeName_;
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
        std::thread processDataTransferThread_;

        /*变量索引: hash -> 元数据*/
        std::unordered_map<uint64_t, VarMetadata> varIndex_;
        mutable std::shared_mutex varIndexMutex_;

        VarStore varStore_; // 变量值存储

        /*写入时间戳/计数，用于回调侧检测数据时效 (lock-free, 单写多读)*/
        struct VarWriteStamp {
            std::atomic<uint64_t> timestampNs{0};
            std::atomic<uint32_t> writeCount{0};
        };
        std::unique_ptr<VarWriteStamp[]> varWriteStamps_;
        uint32_t varWriteStampCount_{0};

        
        /*订阅回调存储: varHash -> 回调信息*/
        std::unordered_map<uint64_t, SubCallbackInfo> subscriptionCallbacks_;
        std::mutex subscriptionCallbacksMutex_;

        /*时间轮回调调度器*/
        std::unique_ptr<TimerScheduler> callbackScheduler_;
        std::mutex callbackGroupsMutex_;
        std::unordered_map<CallbackGroupKey, std::shared_ptr<TimerEventInterface>,
                           CallbackGroupKeyHash>
            callbackGroupTimers_;
        std::unordered_map<CallbackGroupKey, std::shared_ptr<std::vector<CallbackVarInfo>>,
                           CallbackGroupKeyHash>
            callbackGroupMembers_;
        std::thread callbackSchedulerThread_;
        std::atomic<bool> callbackDirty_{false};
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_SUB_V2_H
