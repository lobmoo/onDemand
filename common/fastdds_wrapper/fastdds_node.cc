/**
 * @file fastdds_node.cc
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

#include "fastdds_node.h"
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>

namespace FastddsWrapper
{

using namespace eprosima::fastdds::dds;

FastDataNode::FastDataNode(int domainId, const std::string &participant_name,
                           ParticipantListener *listener)
    : domain_id_(domainId), participant_name_(participant_name)
{
    initialized_ = initDomainParticipant(participant_name, nullptr, listener);
}

FastDataNode::FastDataNode(int domainId, const std::string &participant_name,
                           const ParticipantQoSBuilder &participant_qos,
                           ParticipantListener *listener)
    : domain_id_(domainId), participant_name_(participant_name)
{
    initialized_ = initDomainParticipant(participant_name, &participant_qos, listener);
}

FastDataNode::FastDataNode(const std::string &qosXmlConfig, ParticipantListener *listener)
    : xml_config_path_(qosXmlConfig)
{
    initialized_ = initDomainParticipantForXml(qosXmlConfig, listener);
}

FastDataNode::~FastDataNode()
{
    try {
        cleanup();
    } catch (const std::exception &e) {
        LOG(error) << "Exception in destructor: " << e.what();
    }
}


bool FastDataNode::initDomainParticipant(const std::string &participant_name,
                                         const ParticipantQoSBuilder *participant_qos,
                                         ParticipantListener *listener)
{
    try {
        DomainParticipantQos pqos;

        if (participant_qos != nullptr) {
            pqos = participant_qos->getQos();
        } else {
            pqos = PARTICIPANT_QOS_DEFAULT;
        }

        pqos.name(participant_name);

        participant_ = DomainParticipantFactory::get_instance()->create_participant(
            domain_id_, pqos, listener, StatusMask::none());

        if (participant_ == nullptr) {
            LOG(error) << "Failed to create DomainParticipant";
            return false;
        }

        // 创建Publisher
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
        if (publisher_ == nullptr) {
            LOG(error) << "Failed to create Publisher";
            destroyParticipantResources();
            return false;
        }

        // 创建Subscriber
        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
        if (subscriber_ == nullptr) {
            LOG(error) << "Failed to create Subscriber";
            destroyParticipantResources();
            return false;
        }

        LOG(info) << "FastDataNode initialized successfully with domain ID: " << domain_id_
                  << ", participant name: " << participant_name;
        return true;

    } catch (const std::exception &e) {
        LOG(error) << "Exception during FastDataNode initialization: " << e.what();
        return false;
    }
}

bool FastDataNode::initDomainParticipantForXml(const std::string &qosXmlConfig,
                                               ParticipantListener *listener)
{
    try {
        // 加载XML配置
        if (DomainParticipantFactory::get_instance()->load_XML_profiles_file(qosXmlConfig)
            != eprosima::fastdds::dds::RETCODE_OK) {
            LOG(error) << "Failed to load XML configuration file: " << qosXmlConfig;
            return false;
        }

        // 使用XML中的配置创建Participant
        participant_ = DomainParticipantFactory::get_instance()->create_participant_with_profile(
            "participant_profile", listener, StatusMask::none());

        if (participant_ == nullptr) {
            LOG(error) << "Failed to create DomainParticipant from XML profile";
            return false;
        }

        // 创建Publisher
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
        if (publisher_ == nullptr) {
            LOG(error) << "Failed to create Publisher";
            destroyParticipantResources();
            return false;
        }

        // 创建Subscriber
        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
        if (subscriber_ == nullptr) {
            LOG(error) << "Failed to create Subscriber";
            destroyParticipantResources();
            return false;
        }

        LOG(info) << "FastDataNode initialized from XML config: " << qosXmlConfig;
        return true;

    } catch (const std::exception &e) {
        LOG(error) << "Exception during FastDataNode XML initialization: " << e.what();
        return false;
    }
}

void FastDataNode::addTopicDataTypeCreator(const std::string &topicName,
                                           TopicDataTypeCreator creator)
{
    topic_types_[topicName] = creator;
}

bool FastDataNode::createTopic(const std::string &topicName)
{
    if (topics_.find(topicName) != topics_.end()) {
        return true;
    }

    TypeSupport type_support_ref;
    auto registered_it = registered_topic_types_.find(topicName);
    if (registered_it == registered_topic_types_.end()) {
        TopicDataType *type = getTopicDataType(topicName);
        if (type == nullptr) {
            LOG(error) << "Topic type not registered for: " << topicName;
            return false;
        }

        TypeSupport new_type_support(type);
        if (new_type_support.register_type(participant_) != eprosima::fastdds::dds::RETCODE_OK) {
            LOG(error) << "Failed to register type for topic: " << topicName;
            return false;
        }
        registered_it = registered_topic_types_.emplace(topicName, new_type_support).first;
    }
    type_support_ref = registered_it->second;

    // 创建Topic
    Topic *topic =
        participant_->create_topic(topicName, type_support_ref->get_name(), TOPIC_QOS_DEFAULT);
    if (topic == nullptr) {
        LOG(error) << "Failed to create topic: " << topicName;
        return false;
    }
    topics_[topicName] = topic;
    LOG(debug) << "Created topic: " << topicName << " with type: " << type_support_ref->get_name();
    return true;
}

TopicDataType *FastDataNode::getTopicDataType(const std::string &topicName)
{
    auto it = topic_types_.find(topicName);
    if (it != topic_types_.end()) {
        return it->second();
    }
    return nullptr;
}

bool FastDataNode::isTopicTypeRegistered(const std::string &topicName) const
{
    return topic_types_.find(topicName) != topic_types_.end();
}

void FastDataNode::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查并释放仍然有效的writers
    for (auto& pair : writers_) {
        if (auto writer = pair.second.lock()) {
            LOG(warning) << "DataWriter for topic '" << pair.first 
                         << "' still exists, releasing...";
            writer.reset();
        }
    }
    writers_.clear();

    // 检查并释放仍然有效的readers
    for (auto& pair : readers_) {
        if (auto reader = pair.second.lock()) {
            LOG(warning) << "DataReader for topic '" << pair.first 
                         << "' still exists, releasing...";
            reader.reset();
        }
    }
    readers_.clear();

    for (auto &pair : topics_) {
        if (participant_ != nullptr) {
            participant_->delete_topic(pair.second);
        }
    }
    topics_.clear();
    topic_types_.clear();
    registered_topic_types_.clear();

    destroyParticipantResources();

    LOG(info) << "FastDataNode cleaned up";
}

void FastDataNode::destroyParticipantResources()
{
    if (participant_ == nullptr) {
        return;
    }

    if (publisher_ != nullptr) {
        participant_->delete_publisher(publisher_);
        publisher_ = nullptr;
    }
    if (subscriber_ != nullptr) {
        participant_->delete_subscriber(subscriber_);
        subscriber_ = nullptr;
    }

    DomainParticipantFactory::get_instance()->delete_participant(participant_);
    participant_ = nullptr;
}

} // namespace FastddsWrapper