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

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, DDSDataWriterListener *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "FastDataNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 检查是否存在有效的writer

        auto it = writers_.find(topicName);

        if (it != writers_.end() && !it->second.expired()) {

            LOG(error) << "DataWriter for topic '" << topicName
                       << "' already exists. Use a different topic name or destroy the existing "
                          "writer first.";
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

        auto writerQos = default_writer_qos_.getQos();
        auto topic = topics_[topicName];

        auto writer = std::make_shared<FastddsWrapper::FastDDSTopicWriter<MESSAGE>>(
            publisher_, topic, writerQos, listener);
        writers_[topicName] = writer;

        LOG(info) << "Created DataWriter for topic: " << topicName;
        return writer;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicWriter<MESSAGE>>
    createDataWriter(const std::string topicName, const DataWriterQoSBuilder &writer_qos,
                     DDSDataWriterListener *listener = nullptr)
    {
        if (!initialized_) {
            LOG(error) << "FastDataNode not initialized";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        // 检查是否存在有效的writer

        auto it = writers_.find(topicName);

        if (it != writers_.end() && !it->second.expired()) {

            LOG(error) << "DataWriter for topic '" << topicName
                       << "' already exists. Use a different topic name or destroy the existing "
                          "writer first.";
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
            publisher_, topic, writerQos, listener);
        writers_[topicName] = writer;

        LOG(info) << "Created DataWriter for topic: " << topicName;
        return writer;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<MESSAGE>)> callback,
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
                          "writer first.";

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

        auto readerQos = default_reader_qos_.getQos();

        auto topic = topics_[topicName];

        auto reader = std::make_shared<FastddsWrapper::FastDDSTopicReader<MESSAGE>>(
            subscriber_, topic, callback, readerQos, listener);

        readers_[topicName] = reader;

        LOG(info) << "Created DataReader for topic: " << topicName;
        return reader;
    }

    template <typename MESSAGE, typename PUBSUB_TYPE>
    std::shared_ptr<FastddsWrapper::FastDDSTopicReader<MESSAGE>>
    createDataReader(const std::string topicName,
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
                          "writer first.";

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
            subscriber_, topic, callback, readerQos, listener);

        readers_[topicName] = reader;

        LOG(info) << "Created DataReader for topic: " << topicName;
        return reader;
    }

private:
    bool initDomainParticipant(const std::string &participant_name,
                               const ParticipantQoSBuilder *participant_qos,
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
    eprosima::fastdds::dds::Publisher *publisher_ = nullptr;
    eprosima::fastdds::dds::Subscriber *subscriber_ = nullptr;

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