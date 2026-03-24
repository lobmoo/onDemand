/**
 * @file txdds_topic_writer.hpp
 * @brief TXDDS Topic Writer（结构与 fastdds_topic_writer.hpp 对齐）
 */
#ifndef TXDDS_TOPIC_WRITER_HPP
#define TXDDS_TOPIC_WRITER_HPP

#include <stdexcept>
#include <vector>

#include "txdds/DCPS/common/InstanceHandle.h"
#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/RTPS/common/Duration.h"
#include "txdds/DCPS/publisher/IDataWriter.h"
#include "txdds/DCPS/publisher/IPublisher.h"
#include "txdds/DCPS/topic/ITopic.h"
#include "txdds/DCPS/publisher/qos/DataWriterQos.h"

#include "txdds_listeners.h"
#include "log/logger.h"

namespace TxddsWrapper
{

template <typename T>
class TXDDSTopicWriter
{
public:
    TXDDSTopicWriter(BaoSky::dds::IPublisher *publisher, BaoSky::dds::ITopic *topic,
                     const BaoSky::dds::DataWriterQos &dataWriterQos, DataWriterListener *listener);
    ~TXDDSTopicWriter();

public:
    bool writeMessage(const T &message) const;
    bool clear_history(size_t *removed) const;
    bool wait_for_acknowledgments(const BaoSky::rtps::Duration &max_wait) const;
    bool get_matched_subscriptions(std::vector<BaoSky::dds::InstanceHandle> &subscription_handles);

private:
    BaoSky::dds::IDataWriter *writer_{nullptr};
    BaoSky::dds::IPublisher *publisher_{nullptr};
};

template <typename T>
TXDDSTopicWriter<T>::TXDDSTopicWriter(BaoSky::dds::IPublisher *publisher,
                                      BaoSky::dds::ITopic *topic,
                                      const BaoSky::dds::DataWriterQos &dataWriterQos,
                                      DataWriterListener *listener)
    : writer_(nullptr), publisher_(publisher)
{
    if (publisher_ == nullptr || topic == nullptr) {
        throw std::invalid_argument("TXDDSTopicWriter requires non-null publisher and topic");
    }

    writer_ = publisher_->CreateDataWriter(topic, dataWriterQos, listener);
    if (writer_ == nullptr) {
        throw std::runtime_error("Failed to create TXDDS DataWriter for topic "
                                 + topic->GetTopicName());
    }
}

template <typename T>
TXDDSTopicWriter<T>::~TXDDSTopicWriter()
{
    if (writer_ != nullptr && publisher_ != nullptr) {
        publisher_->DeleteDataWriter(writer_);
    }
}

template <typename T>
bool TXDDSTopicWriter<T>::writeMessage(const T &message) const
{
    if (!writer_) {
        LOG(error) << "TXDDS DataWriter is null, cannot write message";
        return false;
    }
    T *mutable_msg = const_cast<T *>(&message);
    return writer_->Write(mutable_msg) == BaoSky::dds::RETCODE_OK;
}

template <typename T>
bool TXDDSTopicWriter<T>::clear_history(size_t *removed) const
{
    (void)removed;
    LOG(warning) << "TXDDS DataWriter does not support clear_history";
    return false;
}

template <typename T>
bool TXDDSTopicWriter<T>::wait_for_acknowledgments(const BaoSky::rtps::Duration &) const
{
    LOG(warning) << "TXDDS DataWriter does not support wait_for_acknowledgments";
    return false;
}

template <typename T>
bool TXDDSTopicWriter<T>::get_matched_subscriptions(
    std::vector<BaoSky::dds::InstanceHandle> &subscription_handles)
{
    subscription_handles.clear();
    LOG(warning) << "TXDDS DataWriter does not expose matched subscription handles";
    return false;
}

} // namespace TxddsWrapper

#endif // TXDDS_TOPIC_WRITER_HPP