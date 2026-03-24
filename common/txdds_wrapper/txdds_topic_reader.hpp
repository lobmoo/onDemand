/**
 * @file txdds_topic_reader.hpp
 * @brief TXDDS Topic Reader（结构与 fastdds_wrapper 对齐）
 */
#ifndef TXDDS_TOPIC_READER_HPP
#define TXDDS_TOPIC_READER_HPP

#include <functional>
#include <memory>
#include <stdexcept>

#include "txdds/DCPS/subscriber/IDataReader.h"
#include "txdds/DCPS/subscriber/ISubscriber.h"
#include "txdds/DCPS/subscriber/qos/DataReaderQos.h"
#include "txdds/DCPS/topic/ITopic.h"

#include "txdds_listeners.h"

namespace TxddsWrapper
{

template <typename T>
class TXDDSTopicReader
{
    using OnMessageCallback = std::function<void(const std::string &, std::shared_ptr<T>)>;

public:
    TXDDSTopicReader(BaoSky::dds::ISubscriber *subscriber, BaoSky::dds::ITopic *topic,
                     OnMessageCallback callback, const BaoSky::dds::DataReaderQos &dataReaderQos,
                     DataReaderListener<T> *listener);
    ~TXDDSTopicReader();

    TXDDSTopicReader(const TXDDSTopicReader &) = delete;
    TXDDSTopicReader(TXDDSTopicReader &&) = delete;

private:
    DataReaderListener<T> reader_listener_;
    BaoSky::dds::IDataReader *reader_ = nullptr;
    BaoSky::dds::ISubscriber *subscriber_ = nullptr;
};

template <typename T>
TXDDSTopicReader<T>::TXDDSTopicReader(BaoSky::dds::ISubscriber *subscriber,
                                      BaoSky::dds::ITopic *topic, OnMessageCallback callback,
                                      const BaoSky::dds::DataReaderQos &dataReaderQos,
                                      DataReaderListener<T> *listener)
    : subscriber_(subscriber)
{
    if (subscriber_ == nullptr || topic == nullptr) {
        throw std::invalid_argument("TXDDSTopicReader requires non-null subscriber and topic");
    }

    if (nullptr != listener) {
        listener->setMessageCallback(std::move(callback));
        reader_ = subscriber_->CreateDataReader(topic, dataReaderQos, listener);
    } else {
        reader_listener_.setMessageCallback(std::move(callback));
        reader_ = subscriber_->CreateDataReader(topic, dataReaderQos, &reader_listener_);
    }

    if (reader_ == nullptr) {
        throw std::runtime_error("Failed to create TXDDS DataReader for topic "
                                 + topic->GetTopicName());
    }
}

template <typename T>
TXDDSTopicReader<T>::~TXDDSTopicReader()
{
    if (reader_ != nullptr && subscriber_ != nullptr) {
        subscriber_->DeleteDataReader(reader_);
    }
}

} // namespace TxddsWrapper

#endif // TXDDS_TOPIC_READER_HPP
