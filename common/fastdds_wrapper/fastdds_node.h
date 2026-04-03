/**
 * @file fastdds_node.h
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
#ifndef FASTDDS_NODE_H
#define FASTDDS_NODE_H

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <mutex>
#include <functional>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include "fastdds_topic_reader.hpp"
#include "fastdds_topic_writer.hpp"
#include "fastdds_qos_config.h"

#include "log/logger.h"

namespace FastddsWrapper
{

class FastDataNode
{
public:
    using TopicDataTypeCreator = std::function<eprosima::fastdds::dds::TopicDataType *()>;

    using DDSDataWriterListener = FastddsWrapper::DataWriterListener;
    using DomainParticipantListener = FastddsWrapper::ParticipantListener;

    /**
     * @brief 创建数据通信节点(使用默认QoS和监听器)
     * @param domainId domainId
     * @param participant_name participant_name  
     * @param listener 监听器
     */
    FastDataNode(int domainId, const std::string &participant_name,
                 ParticipantListener *listener = nullptr);

    /**
     * @brief 创建数据通信节点(使用自定义QoS)
     * @param domainId domainId
     * @param participant_name participant_name
     * @param participant_qos Participant QoS配置
     * @param listener 监听器
     */
    FastDataNode(int domainId, const std::string &participant_name,
                 const ParticipantQoSBuilder &participant_qos,
                 ParticipantListener *listener = nullptr);

    /**
     * @brief 创建数据通信节点(使用 NodeQoSConfig 完整配置 Participant/Publisher/Subscriber QoS)
     * @param domainId domainId
     * @param participant_name participant_name
     * @param config 节点级 QoS 聚合配置
     * @param listener 监听器
     */
    FastDataNode(int domainId, const std::string &participant_name,
                 const NodeQoSConfig &config,
                 ParticipantListener *listener = nullptr);

    /**
     * @brief 依照配置文件创建数据通信节点
     * @param qosXmlConfig 配置文件路径
     * @param listener 监听器
     */
    FastDataNode(const std::string &qosXmlConfig, ParticipantListener *listener = nullptr);

    ~FastDataNode();

    FastDataNode(const FastDataNode &) = delete;
    FastDataNode &operator=(const FastDataNode &) = delete;
    FastDataNode(FastDataNode &&) = delete;
    FastDataNode &operator=(FastDataNode &&) = delete;

    /**
     * @brief 获取初始化状态
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief 创建额外的 Publisher（支持多 Publisher 场景）
     * @param name Publisher 名称（唯一标识）
     * @param qos Publisher QoS 配置
     * @return true 成功，false 失败（名称冲突或创建失败）
     */
    bool createPublisher(const std::string &name, const PublisherQoSBuilder &qos);

    /**
     * @brief 创建额外的 Subscriber（支持多 Subscriber 场景）
     * @param name Subscriber 名称（唯一标识）
     * @param qos Subscriber QoS 配置
     * @return true 成功，false 失败（名称冲突或创建失败）
     */
    bool createSubscriber(const std::string &name, const SubscriberQoSBuilder &qos);

    /**
     * @brief 强制发送 PDP 心跳，确保 participant 的存在已被通告
     */
    void assertLiveliness()
    {
        if (participant_) {
            participant_->assert_liveliness();
        }
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, DDSDataWriterListener *listener = nullptr)
    {
        return createDataWriter<MESSAGE, PUBSUB_TYPE>(topicName, "default",
                                                       default_writer_qos_, listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, const DataWriterQoSBuilder &writer_qos,
                     DDSDataWriterListener *listener = nullptr)
    {
        return createDataWriter<MESSAGE, PUBSUB_TYPE>(topicName, "default",
                                                       writer_qos, listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, const std::string &publisher_name,
                     const DataWriterQoSBuilder &writer_qos,
                     DDSDataWriterListener *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "FastDataNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = writers_.find(topicName);
        if (it != writers_.end() && !it->second.expired()) {
            LOG(error) << "DataWriter for topic '" << topicName
                       << "' already exists. Use a different topic name or destroy the existing "
                          "writer first.";
            return nullptr;
        }

        // 查找指定的 publisher
        auto pub_it = publishers_.find(publisher_name);
        if (pub_it == publishers_.end()) {
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

        auto writer = std::make_shared<FastddsWrapper::FastDDSTopicWriter<MESSAGE>>(
            pub_it->second, topic, writerQos, listener);
        writers_[topicName] = writer;

        LOG(info) << "Created DataWriter for topic: " << topicName
                  << " on publisher: " << publisher_name;
        return writer;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
                     DataReaderListener<MESSAGE> *listener = nullptr)
    {
        return createDataReader<MESSAGE, PUBSUB_TYPE>(topicName, "default",
                                                       callback, default_reader_qos_, listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
                     const DataReaderQoSBuilder &reader_qos,
                     DataReaderListener<MESSAGE> *listener = nullptr)
    {
        return createDataReader<MESSAGE, PUBSUB_TYPE>(topicName, "default",
                                                       callback, reader_qos, listener);
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName, const std::string &subscriber_name,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
                     const DataReaderQoSBuilder &reader_qos,
                     DataReaderListener<MESSAGE> *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "FastDataNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = readers_.find(topicName);
        if (it != readers_.end() && !it->second.expired()) {
            LOG(error) << "DataReader for topic '" << topicName
                       << "' already exists. Use a different topic name or destroy the existing "
                          "reader first.";
            return nullptr;
        }

        // 查找指定的 subscriber
        auto sub_it = subscribers_.find(subscriber_name);
        if (sub_it == subscribers_.end()) {
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

        auto reader = std::make_shared<FastddsWrapper::FastDDSTopicReader<MESSAGE>>(
            sub_it->second, topic, callback, readerQos, listener);

        readers_[topicName] = reader;

        LOG(info) << "Created DataReader for topic: " << topicName
                  << " on subscriber: " << subscriber_name;
        return reader;
    }

private:
    bool initDomainParticipant(const std::string &participant_name,
                               const ParticipantQoSBuilder *participant_qos,
                               const PublisherQoSBuilder *publisher_qos,
                               const SubscriberQoSBuilder *subscriber_qos,
                               ParticipantListener *listener);
    bool initDomainParticipantForXml(const std::string &qosXmlConfig,
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
    eprosima::fastdds::dds::TopicDataType *getTopicDataType(const std::string &topicName);
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

    // FastDDS核心对象
    eprosima::fastdds::dds::DomainParticipant *participant_ = nullptr;
    std::unordered_map<std::string, eprosima::fastdds::dds::Publisher *> publishers_;
    std::unordered_map<std::string, eprosima::fastdds::dds::Subscriber *> subscribers_;

    // Topic管理
    std::unordered_map<std::string, eprosima::fastdds::dds::Topic *> topics_;
    std::unordered_map<std::string, TopicDataTypeCreator> topic_types_;
    std::unordered_map<std::string, eprosima::fastdds::dds::TypeSupport> registered_topic_types_;

    // 对象管理（使用weak_ptr，不干预生命周期）
    std::unordered_map<std::string, std::weak_ptr<void>> writers_;
    std::unordered_map<std::string, std::weak_ptr<void>> readers_;

    // 线程安全
    mutable std::mutex mutex_;
};
} // namespace FastddsWrapper

#endif