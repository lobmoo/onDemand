/**
 * @file txdds_node.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-11-21     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef TXDDS_NODE_H
#define TXDDS_NODE_H

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <typeinfo>

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

    /**
     * @brief 创建数据通信节点(使用默认QoS和监听器)
     * @param domainId domainId
     * @param participant_name participant_name  
     * @param listener 监听器
     */
    TXDDSNode(int domainId, const std::string &participant_name,
              ParticipantListener *listener = nullptr);

    /**
     * @brief 创建数据通信节点(使用自定义QoS)
     * @param domainId domainId
     * @param participant_name participant_name  
     * @param participant_qos Participant QoS配置
     * @param listener 监听器
     */
    TXDDSNode(int domainId, const std::string &participant_name,
              const ParticipantQoSBuilder &participant_qos,
              ParticipantListener *listener = nullptr);

    /**
     * @brief 依照配置文件创建数据通信节点（TXDDS暂不支持，保留接口保持结构一致）
     * @param qosXmlConfig 配置文件路径
     * @param listener 监听器
     */
    TXDDSNode(const std::string &qosXmlConfig, ParticipantListener *listener = nullptr);

    ~TXDDSNode();

    TXDDSNode(const TXDDSNode &) = delete;
    TXDDSNode &operator=(const TXDDSNode &) = delete;
    TXDDSNode(TXDDSNode &&) = delete;
    TXDDSNode &operator=(TXDDSNode &&) = delete;
    void disableListener() { participant_->SetListener(nullptr, 0); }
    /** 将所有 DataReader 的 listener 置空（TXDDS 若需可后续实现） */
    void disableAllDataReaderListeners() {}
    /**
     * @brief 获取初始化状态
     */
    bool isInitialized() const { return initialized_; }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, DDSDataWriterListener *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "TXDDSNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (writers_.find(topicName) != writers_.end()) {
            if (writers_[topicName].use_count() > 0) {
                LOG(error)
                    << "DataWriter for topic '" << topicName
                    << "' already exists. Use a different topic name or destroy the existing "
                       "writer first.";
                return nullptr;
            }
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

        auto writerQos = default_writer_qos_.getQos();
        auto topic = topics_[topicName];

        auto writer = std::make_shared<TxddsWrapper::TXDDSTopicWriter<MESSAGE>>(
            publisher_, topic, writerQos, listener);
        writers_[topicName] = writer;

        LOG(info) << "Created DataWriter for topic: " << topicName;
        return writer;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, const DataWriterQoSBuilder &writer_qos,
                     DDSDataWriterListener *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "TXDDSNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (writers_.find(topicName) != writers_.end()) {
            if (writers_[topicName].use_count() > 0) {
                LOG(error)
                    << "DataWriter for topic '" << topicName
                    << "' already exists. Use a different topic name or destroy the existing "
                       "writer first.";
                return nullptr;
            }
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
            publisher_, topic, writerQos, listener);
        writers_[topicName] = writer;

        LOG(info) << "Created DataWriter for topic: " << topicName;
        return writer;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
                     DataReaderListener<MESSAGE> *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "TXDDSNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (readers_.find(topicName) != readers_.end()) {
            if (readers_[topicName].use_count() > 0) {
                LOG(error)
                    << "DataReader for topic '" << topicName
                    << "' already exists. Use a different topic name or destroy the existing "
                       "reader first.";
                return nullptr;
            }
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

        auto readerQos = default_reader_qos_.getQos();

        auto topic = topics_[topicName];

        auto reader = std::make_shared<TxddsWrapper::TXDDSTopicReader<MESSAGE>>(
            subscriber_, topic, callback, readerQos, listener);

        readers_[topicName] = reader;

        LOG(info) << "Created DataReader for topic: " << topicName;
        return reader;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<TxddsWrapper::TXDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
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
                LOG(error)
                    << "DataReader for topic '" << topicName
                    << "' already exists. Use a different topic name or destroy the existing "
                       "reader first.";
                return nullptr;
            }
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
            subscriber_, topic, callback, readerQos, listener);

        readers_[topicName] = reader;

        LOG(info) << "Created DataReader for topic: " << topicName;
        return reader;
    }

private:
    bool initDomainParticipant(const std::string &participant_name,
                               const ParticipantQoSBuilder *participant_qos,
                               ParticipantListener *listener);

    template <typename T>
    void registerTopicType(const std::string &topicName)
    {
        /*这里 new的裸指针会被TypeSupport构造包装成智能指针*/
        addTopicDataTypeCreator(topicName, []() { return new T(); });
    }

    void addTopicDataTypeCreator(const std::string &topicName, TopicDataTypeCreator creator);

    // 核心方法
    bool createTopic(const std::string &topicName);
    BaoSky::dds::TopicDataType *getTopicDataType(const std::string &topicName);
    bool isTopicTypeRegistered(const std::string &topicName) const;
    void cleanup();
    void destroyParticipantResources();

    // 配置相关
    int domain_id_ = 0;
    std::string participant_name_;
    std::string xml_config_path_;
    bool initialized_ = false;

    // 默认QoS配置（可以通过setter修改）
    DataWriterQoSBuilder default_writer_qos_ = QoSPresets::defaultWriter();
    DataReaderQoSBuilder default_reader_qos_ = QoSPresets::defaultReader();

    // TXDDS核心对象
    BaoSky::dds::IDomainParticipant *participant_ = nullptr;
    BaoSky::dds::IPublisher *publisher_ = nullptr;
    BaoSky::dds::ISubscriber *subscriber_ = nullptr;

    // Topic管理
    std::unordered_map<std::string, BaoSky::dds::ITopic *> topics_;
    std::unordered_map<std::string, TopicDataTypeCreator> topic_types_;
    std::unordered_map<std::string, BaoSky::dds::TypeSupport> registered_topic_types_;

    // 对象管理
    std::unordered_map<std::string, std::weak_ptr<void>> writers_;
    std::unordered_map<std::string, std::weak_ptr<void>> readers_;

    // 线程安全
    mutable std::mutex mutex_;
};
} // namespace TxddsWrapper

#endif
