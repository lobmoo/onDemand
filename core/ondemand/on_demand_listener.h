/**
 * @file on_demand_listener.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2026-06-05
 * 
 * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2026-06-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef ON_DEMAND_LISTENER_H
#define ON_DEMAND_LISTENER_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "dds_wrapper/dds_abstraction.h"

namespace dsf
{
namespace ondemand
{

    /**
 * @brief 统一的 DDS 事件监听器
 *
 * 继承 ParticipantListener + DataWriterListener，集中处理三类事件：
 *   1. onParticipantDiscovery  — participant 上下线
 *   2. onPublicationMatched    — writer 匹配到 reader
 *   3. onSubscriptionMatched   — reader 匹配到 writer
 *
 * 子类 override 这三个方法做业务逻辑。
 * 外部用户通过 setXxxCallback() 注册回调，参数统一为 (topicName, currentCount, change, totalCount)。
 * 创建 DataReader 时调 createReaderListener<T>() 获取 listener 指针。
 */
    class OnDemandListener : public DdsWrapper::ParticipantListener,
                             public DdsWrapper::DataWriterListener
    {
    public:
        // 回调类型：topic名、当前匹配数、变化量、累计总数
        using MatchedCallback = std::function<void(const std::string &topicName, int currentCount,
                                                   int currentCountChange, int totalCount)>;

        OnDemandListener() = default;
        ~OnDemandListener() override = default;

        OnDemandListener(const OnDemandListener &) = delete;
        OnDemandListener &operator=(const OnDemandListener &) = delete;

        // ---- 回调注册（给用户用） ----

        void setOnPublicationMatchedCallback(MatchedCallback cb) { pubMatchedCb_ = std::move(cb); }

        void setOnSubscriptionMatchedCallback(MatchedCallback cb) { subMatchedCb_ = std::move(cb); }

        // ---- DDS 回调入口（子类可 override 扩展） ----

        virtual void onParticipantDiscovery(const DdsWrapper::ParticipantInfo &info) { (void)info; }

        void onPublicationMatched(const DdsWrapper::MatchedInfo &info) override
        {

            LOG(info) << "Publication matched: topic=" << info.topic_name
                      << ", currentCount=" << info.current_count
                      << ", change=" << info.current_count_change
                      << ", totalCount=" << info.total_count;
            if (pubMatchedCb_) {
                pubMatchedCb_(info.topic_name, info.current_count, info.current_count_change,
                              info.total_count);
            }
        }

        virtual void onSubscriptionMatched(const DdsWrapper::MatchedInfo &info)
        {
            LOG(info) << "Subscription matched: topic=" << info.topic_name
                      << ", currentCount=" << info.current_count
                      << ", change=" << info.current_count_change
                      << ", totalCount=" << info.total_count;
            if (subMatchedCb_) {
                subMatchedCb_(info.topic_name, info.current_count, info.current_count_change,
                              info.total_count);
            }
        }

        // ---- Reader Listener 工厂（声明，定义在 OndemandDataReaderListener 之后） ----

        template <typename T>
        DdsWrapper::DataReaderListener<T> *createReaderListener();

    private:
        std::vector<std::shared_ptr<void>> readerListeners_;
        MatchedCallback pubMatchedCb_;
        MatchedCallback subMatchedCb_;
    };

    /**
 * @brief DataReader 监听器（模板转发层）
 *
 * 每个 DataReader 创建一个实例，onSubscriptionMatched 转发到宿主。
 * 生命周期由 OnDemandListener::createReaderListener<T>() 管理。
 */
    template <typename T>
    class OndemandDataReaderListener : public DdsWrapper::DataReaderListener<T>
    {
    public:
        explicit OndemandDataReaderListener(OnDemandListener *host) : host_(host) {}
        ~OndemandDataReaderListener() override = default;

    protected:
        void onSubscriptionMatched(const DdsWrapper::MatchedInfo &info) override
        {
            if (host_) {
                host_->onSubscriptionMatched(info);
            }
        }

    private:
        OnDemandListener *host_;
    };

    // ---- 模板方法定义（在 OndemandDataReaderListener 完整定义之后） ----

    template <typename T>
    DdsWrapper::DataReaderListener<T> *OnDemandListener::createReaderListener()
    {
        auto ptr = std::make_shared<OndemandDataReaderListener<T>>(this);
        readerListeners_.push_back(ptr);
        return ptr.get();
    }

} // namespace ondemand
} // namespace dsf

#endif // ON_DEMAND_LISTENER_H
