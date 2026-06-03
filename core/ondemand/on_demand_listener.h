#ifndef ON_DEMAND_LISTENER_H
#define ON_DEMAND_LISTENER_H

#include <memory>
#include <vector>
#include "dds_wrapper/dds_abstraction.h"
#include "log/logger.h"

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
 * 创建 DataReader 时调 createReaderListener<T>() 获取 listener 指针。
 */
class OnDemandListener : public DdsWrapper::ParticipantListener,
                          public DdsWrapper::DataWriterListener
{
public:
    OnDemandListener() = default;
    ~OnDemandListener() override = default;

    OnDemandListener(const OnDemandListener &) = delete;
    OnDemandListener &operator=(const OnDemandListener &) = delete;

    // ---- 业务入口（子类 override） ----

    virtual void onParticipantDiscovery(const DdsWrapper::ParticipantInfo &info)
    {
        (void)info;
    }

    void onPublicationMatched(const DdsWrapper::MatchedInfo &info) override
    {
        ONDEMANDLOG(critical) << "onPublicationMatched: topic=" << info.topic_name
                          << " current=" << info.current_count
                          << " change=" << info.current_count_change
                          << " total=" << info.total_count;
    }

    virtual void onSubscriptionMatched(const DdsWrapper::MatchedInfo &info)
    {
        ONDEMANDLOG(critical) << "onSubscriptionMatched: topic=" << info.topic_name
                          << " current=" << info.current_count
                          << " change=" << info.current_count_change
                          << " total=" << info.total_count;
    }

    // ---- Reader Listener 工厂（声明，定义在 OndemandDataReaderListener 之后） ----

    template <typename T>
    DdsWrapper::DataReaderListener<T> *createReaderListener();

private:
    std::vector<std::shared_ptr<void>> readerListeners_;
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
