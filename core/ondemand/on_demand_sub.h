#ifndef ON_DEMAND_SUB_H
#define ON_DEMAND_SUB_H

#include "on_demand_common.h"
#include "concurrentqueue.h"

namespace dsf
{
namespace ondemand
{

    /**
 * @brief 数据回调函数
 */
    using DataCallback =
        std::function<void(const std::string &varName, const void *data, size_t size)>;

    /**
 * @brief 按需订阅器 V2 - 重构版本
 */
    class OnDemandSub
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
     * @brief 订阅变量
     */
        bool subscribe(const std::string &varName, const std::string &tableName,
                       uint32_t frequency);

        /**
     * @brief 批量订阅变量
     */
        size_t batchSubscribe();

        /**
     * @brief 取消订阅
     */
        bool unsubscribe(const std::string &varName);

        /**
     * @brief 获取订阅数量
     */
        size_t getSubscriptionCount() const;

        /**
     * @brief 导出状态
     */
        void dumpState(std::ostream &os);

    private:
        /**
       * @brief 创建变量定义数据读取器
       * @param  processFunc      MyParamDoc
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
         * @brief 处理变量定义数据回调函数
         * @param  topicName        
         * @param  data             
         * @return true 
         * @return false 
         */
        bool onReceiveTableDefine(const std::string &topicName,
                                  std::shared_ptr<DSF::Var::PubTableDefine> data);

        /**
        * @brief 处理变量定义数据
        */
        void processTableDefine();

    private:
        std::string nodeName_;
        std::shared_ptr<DdsWrapper::DataNode> dataNode_;

        std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Var::PubTableDefine>>
            pubTableDefineReader_; // 变量定义数据读取器

        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Message::SubTableRegister>>
            subTableRegisterReqWriter_; // 频率请求数据写入器

        std::atomic<bool> initialized_;
        std::atomic<bool> running_;

        std::atomic<uint64_t> totalReceived_;

        /*变量定义队列*/
        moodycamel::ConcurrentQueue<std::shared_ptr<DSF::Var::PubTableDefine>> pubTableDefineQueue_;

        /*处理线程*/
        std::thread processTableDefineThread_;
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_SUB_V2_H
