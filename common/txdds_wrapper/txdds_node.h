/**
 * @file txdds_node.h
 * @brief
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025 by wwk : wwk.lobmo@gmail.com
 */
#ifndef TXDDS_NODE_H
#define TXDDS_NODE_H

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>

#include "txdds/DCPS/domain/DomainParticipantFactory.h"
#include "txdds/DCPS/domain/qos/DomainParticipantQos.h"
#include "txdds/DCPS/publisher/IPublisher.h"
#include "txdds/DCPS/subscriber/ISubscriber.h"
#include "txdds/DCPS/topic/ITopic.h"
#include "txdds/DCPS/topic/TopicDataType.h"
#include "txdds/DCPS/topic/TypeSupport.h"
#include "txdds/DCPS/topic/qos/TopicQos.h"

#include "txdds_topic_reader.hpp"
#include "txdds_topic_writer.hpp"
#include "txdds_qos_config.h"

#include "log/logger.h"

namespace TxddsWrapper
{

class TXDDSNode
{
public:
    using TopicDataTypeCreator = std::function<BaoSky::dds::TopicDataType *()>;

    using DDSDataWriterListener = TxddsWrapper::DataWriterListener;
    using DomainParticipantListener = TxddsWrapper::ParticipantListener;

    TXDDSNode(int domainId, const std::string &participant_name,
              ParticipantListener *listener = nullptr);

    TXDDSNode(int domainId, const std::string &participant_name,
              const ParticipantQoSBuilder &participant_qos,
              ParticipantListener *listener = nullptr);

    TXDDSNode(const std::string &qosXmlConfig, ParticipantListener *listener = nullptr);

    ~TXDDSNode();

    TXDDSNode(const TXDDSNode &) = delete;
    TXDDSNode &operator=(const TXDDSNode &) = delete;
    TXDDSNode(TXDDSNode &&) = delete;
    TXDDSNode &operator=(TXDDSNode &&) = delete;

    void disableListener()
    {
        if (participant_ != nullptr) {
            participant_->SetListener(nullptr, 0);
        }
    }

    void disableAllDataReaderListeners() {}

    bool isInitialized() const { return initialized_; }

    void assertLiveliness() {}

    // FastDDS-compatible entity APIs
    bool createPublisher(const std::string &name, const PublisherQoSBuilder &qos);
    bool createSubscriber(const std::string &name, const SubscriberQoSBuilder &qos);
    bool updatePublisherQos(const std::string &name, const PublisherQoSBuilder &qos);
    bool updateSubscriberQos(const std::string &name, const SubscriberQoSBuilder &qos);

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, DDSDataWriterListener *listener = nullptr)
    {
        return createDataWriter<MESSAGE, PUBSUB_TYPE>(topicName, "default", default_writer_qos_,
                                                       listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, const DataWriterQoSBuilder &writer_qos,
                     DDSDataWriterListener *listener = nullptr)
    {
        return createDataWriter<MESSAGE, PUBSUB_TYPE>(topicName, "default", writer_qos, listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, const std::string &publisher_name,
                     const DataWriterQoSBuilder &writer_qos,
                     DDSDataWriterListener *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "TXDDSNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (writers_.find(topicName) != writers_.end()) {
            if (writers_[topicName].use_count() > 0) {
                LOG(error) << "DataWriter for topic '" << topicName
                           << "' already exists. Use a different topic name or destroy the existing "
                              "writer first.";
                return nullptr;
            }
        }

        auto pub_it = publishers_.find(publisher_name);
        if (pub_it == publishers_.end() || pub_it->second == nullptr) {
            LOG(error) << "Publisher '" << publisher_name << "' not found";
            return nullptr;
        }

        if (!isTopicTypeRegistered(topicName)) {
            addTopicDataTypeCreator(topicName, []() { return new PUBSUB_TYPE(); });
            LOG(debug) << "Auto-registered " << typeid(PUBSUB_TYPE).name()
                       << "PubSubType for topic: " << topicName;
        }

        if (!createTopic(topicName)) {
            LOG(error) << "Failed to create topic: " << topicName;
            return nullptr;
        }

        auto writerQos = writer_qos.getQos();
        auto topic = topics_[topicName];

        auto writer = std::make_shared<TxddsWrapper::TXDDSTopicWriter<MESSAGE>>(
            pub_it->second, topic, writerQos, listener);
        writers_[topicName] = writer;

        LOG(info) << "Created DataWriter for topic: " << topicName
                  << " on publisher: " << publisher_name;
        return writer;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
                     DataReaderListener<MESSAGE> *listener = nullptr)
    {
        return createDataReader<MESSAGE, PUBSUB_TYPE>(topicName, "default", std::move(callback),
                                                       default_reader_qos_, listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
                     const DataReaderQoSBuilder &reader_qos,
                     DataReaderListener<MESSAGE> *listener = nullptr)
    {
        return createDataReader<MESSAGE, PUBSUB_TYPE>(topicName, "default", std::move(callback),
                                                       reader_qos, listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName, const std::string &subscriber_name,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
                     const DataReaderQoSBuilder &reader_qos,
                     DataReaderListener<MESSAGE> *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "TXDDSNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (readers_.find(topicName) != readers_.end()) {
            if (readers_[topicName].use_count() > 0) {
                LOG(error) << "DataReader for topic '" << topicName
                           << "' already exists. Use a different topic name or destroy the existing "
                              "reader first.";
                return nullptr;
            }
        }

        auto sub_it = subscribers_.find(subscriber_name);
        if (sub_it == subscribers_.end() || sub_it->second == nullptr) {
            LOG(error) << "Subscriber '" << subscriber_name << "' not found";
            return nullptr;
        }

        if (!isTopicTypeRegistered(topicName)) {
            addTopicDataTypeCreator(topicName, []() { return new PUBSUB_TYPE(); });
            LOG(debug) << "Auto-registered " << typeid(PUBSUB_TYPE).name()
                       << "PubSubType for topic: " << topicName;
        }

        if (!createTopic(topicName)) {
            LOG(error) << "Failed to create topic: " << topicName;
            return nullptr;
        }

        auto readerQos = reader_qos.getQos();
        auto topic = topics_[topicName];

        auto reader = std::make_shared<TxddsWrapper::TXDDSTopicReader<MESSAGE>>(
            sub_it->second, topic, std::move(callback), readerQos, listener);

        readers_[topicName] = reader;

        LOG(info) << "Created DataReader for topic: " << topicName
                  << " on subscriber: " << subscriber_name;
        return reader;
    }

private:
    bool initDomainParticipant(const std::string &participant_name,
                               const ParticipantQoSBuilder *participant_qos,
                               ParticipantListener *listener);

    template <typename T>
    void registerTopicType(const std::string &topicName)
    {
        addTopicDataTypeCreator(topicName, []() { return new T(); });
    }

    void addTopicDataTypeCreator(const std::string &topicName, TopicDataTypeCreator creator);

    bool createTopic(const std::string &topicName);
    BaoSky::dds::TopicDataType *getTopicDataType(const std::string &topicName);
    bool isTopicTypeRegistered(const std::string &topicName) const;
    void cleanup();
    void destroyParticipantResources();

    int domain_id_ = 0;
    std::string participant_name_;
    std::string xml_config_path_;
    bool initialized_ = false;

    DataWriterQoSBuilder default_writer_qos_ = QoSPresets::defaultWriter();
    DataReaderQoSBuilder default_reader_qos_ = QoSPresets::defaultReader();

    BaoSky::dds::IDomainParticipant *participant_ = nullptr;
    std::unordered_map<std::string, BaoSky::dds::IPublisher *> publishers_;
    std::unordered_map<std::string, BaoSky::dds::ISubscriber *> subscribers_;

    std::unordered_map<std::string, BaoSky::dds::ITopic *> topics_;
    std::unordered_map<std::string, TopicDataTypeCreator> topic_types_;
    std::unordered_map<std::string, BaoSky::dds::TypeSupport> registered_topic_types_;

    std::unordered_map<std::string, std::weak_ptr<void>> writers_;
    std::unordered_map<std::string, std::weak_ptr<void>> readers_;

    mutable std::mutex mutex_;
};
} // namespace TxddsWrapper

#endif
