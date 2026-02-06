#ifndef ON_DEMAND_SUB_H
#define ON_DEMAND_SUB_H

#include "on_demand_common.h"

namespace dsf
{
namespace ondemand
{

    /**
 * @brief 订阅项
 */
    struct SubscriptionItem {
        std::string varName;
        std::string tableName;
        uint64_t varHash;
        uint32_t frequency;

        SubscriptionItem(const std::string &vn, const std::string &tn, uint32_t freq)
            : varName(vn), tableName(tn), varHash(fast_hash(vn)), frequency(freq)
        {
        }
    };

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
        size_t batchSubscribe(const std::vector<SubscriptionItem> &items);

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
        bool createTableDefineReader(
            std::function<void(const std::string &, std::shared_ptr<DSF::Var::PubTableDefine>)>
                processFunc);
        bool createSubTableRegisterWriter();
        bool onReceiveTableDefine(const std::string &topicName,
                                  std::shared_ptr<DSF::Var::PubTableDefine> data);

    private:
        std::unordered_map<uint64_t, SubscriptionItem> subscriptions_;
        mutable std::shared_mutex subMutex_;

        std::string nodeName_;
        std::shared_ptr<DdsWrapper::DataNode> dataNode_;

        std::shared_ptr<DdsWrapper::DDSTopicReader<DSF::Var::PubTableDefine>>
            pubTableDefineReader_; // 变量定义数据读取器

        std::shared_ptr<DdsWrapper::DDSTopicWriter<DSF::Message::SubTableRegister>>
            subTableRegisterReqWriter_; // 频率请求数据写入器

        std::atomic<bool> initialized_;
        std::atomic<bool> running_;

        std::atomic<uint64_t> totalReceived_;
    };

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_SUB_V2_H
