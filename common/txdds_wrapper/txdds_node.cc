#include "txdds_node.h"

#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/domain/DomainParticipantFactory.h"
#include "txdds/DCPS/publisher/qos/PublisherQos.h"
#include "txdds/DCPS/subscriber/qos/SubscriberQos.h"
#include "txdds/DCPS/topic/qos/TopicQos.h"
#include "txdds/RTPS/transport/ThreadManager.h"
#include "txdds/RTPS/transport/TransportStack.h"
#include "txdds/RTPS/transport/TransportConfig.h"

namespace TxddsWrapper
{

TXDDSNode::TXDDSNode(int domainId,
                     const std::string &participant_name,
                     ParticipantListener *listener)
    : domain_id_(domainId), participant_name_(participant_name)
{
    initialized_ = initDomainParticipant(participant_name, nullptr, listener);
    if (!initialized_) {
        throw std::runtime_error("Failed to initialize TXDDSNode");
    }
}

TXDDSNode::TXDDSNode(int domainId, const std::string &participant_name,
                     const ParticipantQoSBuilder &participant_qos, ParticipantListener *listener)
    : domain_id_(domainId), participant_name_(participant_name)
{
    initialized_ = initDomainParticipant(participant_name, &participant_qos, listener);
    if (!initialized_) {
        throw std::runtime_error("Failed to initialize TXDDSNode");
    }
}

TXDDSNode::TXDDSNode(const std::string &qosXmlConfig, ParticipantListener *listener)
    : xml_config_path_(qosXmlConfig)
{
    (void)listener;
    LOG(error) << "TXDDSNode does not support XML based initialization yet: " << qosXmlConfig;
    initialized_ = false;
}

TXDDSNode::~TXDDSNode()
{
    try {
        cleanup();
    } catch (const std::exception &e) {
        LOG(error) << "Exception in TXDDSNode destructor: " << e.what();
    }
}

bool TXDDSNode::initDomainParticipant(const std::string &participant_name,
                                      const ParticipantQoSBuilder *participant_qos,
                                      ParticipantListener *listener)
{
    try {
        BaoSky::dds::DomainParticipantQos pqos;
        if (participant_qos != nullptr) {
            pqos = participant_qos->getQos();
        } else {
            pqos = QoSPresets::defaultParticipant().getQos();
        }
        pqos.mName = participant_name;

        auto factory = BaoSky::dds::DomainParticipantFactory::GetInstance();
        participant_ = factory->CreateParticipant(domain_id_, pqos, listener);
        if (participant_ == nullptr) {
            LOG(error) << "Failed to create TXDDS DomainParticipant";
            return false;
        }

        BaoSky::dds::PublisherQos publisher_qos = QoSPresets::defaultPublisher().getQos();
        auto *default_pub = participant_->CreatePublisher(publisher_qos);
        if (default_pub == nullptr) {
            LOG(error) << "Failed to create TXDDS default Publisher";
            destroyParticipantResources();
            return false;
        }
        publishers_["default"] = default_pub;

        BaoSky::dds::SubscriberQos subscriber_qos = QoSPresets::defaultSubscriber().getQos();
        auto *default_sub = participant_->CreateSubscriber(subscriber_qos);
        if (default_sub == nullptr) {
            LOG(error) << "Failed to create TXDDS default Subscriber";
            destroyParticipantResources();
            return false;
        }
        subscribers_["default"] = default_sub;

        initialized_ = true;
        LOG(info) << "TXDDSNode initialized successfully with domain ID: " << domain_id_
                  << ", participant name: " << participant_name_;
        return true;

    } catch (const std::exception &e) {
        LOG(error) << "Exception during TXDDSNode initialization: " << e.what();
        destroyParticipantResources();
        return false;
    }
}

bool TXDDSNode::createPublisher(const std::string &name, const PublisherQoSBuilder &qos)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (publishers_.find(name) != publishers_.end()) {
        LOG(error) << "Publisher '" << name << "' already exists";
        return false;
    }

    if (participant_ == nullptr) {
        LOG(error) << "Participant not initialized";
        return false;
    }

    auto pub_qos = qos.getQos();
    auto *publisher = participant_->CreatePublisher(pub_qos);
    if (publisher == nullptr) {
        LOG(error) << "Failed to create Publisher '" << name << "'";
        return false;
    }

    publishers_[name] = publisher;
    LOG(info) << "Created Publisher: " << name;
    return true;
}

bool TXDDSNode::createSubscriber(const std::string &name, const SubscriberQoSBuilder &qos)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (subscribers_.find(name) != subscribers_.end()) {
        LOG(error) << "Subscriber '" << name << "' already exists";
        return false;
    }

    if (participant_ == nullptr) {
        LOG(error) << "Participant not initialized";
        return false;
    }

    auto sub_qos = qos.getQos();
    auto *subscriber = participant_->CreateSubscriber(sub_qos);
    if (subscriber == nullptr) {
        LOG(error) << "Failed to create Subscriber '" << name << "'";
        return false;
    }

    subscribers_[name] = subscriber;
    LOG(info) << "Created Subscriber: " << name;
    return true;
}

bool TXDDSNode::updatePublisherQos(const std::string &name, const PublisherQoSBuilder &qos)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = publishers_.find(name);
    if (it == publishers_.end() || it->second == nullptr) {
        LOG(error) << "Publisher '" << name << "' not found";
        return false;
    }

    auto rc = it->second->UpdateQos(qos.getQos());
    if (rc != BaoSky::dds::RETCODE_OK) {
        LOG(error) << "Failed to update Publisher QoS for '" << name
                   << "', ret=" << static_cast<int>(rc);
        return false;
    }

    LOG(info) << "Updated Publisher QoS for: " << name;
    return true;
}

bool TXDDSNode::updateSubscriberQos(const std::string &name, const SubscriberQoSBuilder &qos)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = subscribers_.find(name);
    if (it == subscribers_.end() || it->second == nullptr) {
        LOG(error) << "Subscriber '" << name << "' not found";
        return false;
    }

    auto rc = it->second->UpdateQos(qos.getQos());
    if (rc != BaoSky::dds::RETCODE_OK) {
        LOG(error) << "Failed to update Subscriber QoS for '" << name
                   << "', ret=" << static_cast<int>(rc);
        return false;
    }

    LOG(info) << "Updated Subscriber QoS for: " << name;
    return true;
}

void TXDDSNode::addTopicDataTypeCreator(const std::string &topicName, TopicDataTypeCreator creator)
{
    topic_types_[topicName] = std::move(creator);
}

bool TXDDSNode::createTopic(const std::string &topicName)
{
    if (topics_.find(topicName) != topics_.end()) {
        return true;
    }

    BaoSky::dds::TypeSupport cached_type;
    auto it = registered_topic_types_.find(topicName);
    if (it == registered_topic_types_.end()) {
        BaoSky::dds::TopicDataType *type = getTopicDataType(topicName);
        if (type == nullptr) {
            LOG(error) << "Topic type not registered for: " << topicName;
            return false;
        }

        BaoSky::dds::TypeSupport new_type_support(type);
        if (new_type_support.RegisterType(participant_) != BaoSky::dds::RETCODE_OK) {
            LOG(error) << "Failed to register type for topic: " << topicName;
            return false;
        }
        it = registered_topic_types_.emplace(topicName, new_type_support).first;
    }
    cached_type = it->second;

    BaoSky::dds::TopicQos topic_qos;
    BaoSky::dds::ITopic *topic =
        participant_->CreateTopic(topicName, cached_type->mTypeName, topic_qos);
    if (topic == nullptr) {
        LOG(error) << "Failed to create topic: " << topicName;
        return false;
    }

    topics_[topicName] = topic;
    LOG(debug) << "Created topic: " << topicName << " with type: " << cached_type->mTypeName;
    return true;
}

BaoSky::dds::TopicDataType *TXDDSNode::getTopicDataType(const std::string &topicName)
{
    auto it = topic_types_.find(topicName);
    if (it != topic_types_.end()) {
        return it->second();
    }
    return nullptr;
}

bool TXDDSNode::isTopicTypeRegistered(const std::string &topicName) const
{
    return topic_types_.find(topicName) != topic_types_.end();
}

void TXDDSNode::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);

    writers_.clear();
    readers_.clear();

    for (auto &entry : topics_) {
        if (participant_ != nullptr && entry.second != nullptr) {
            participant_->DeleteTopic(entry.second);
        }
    }
    topics_.clear();
    topic_types_.clear();
    registered_topic_types_.clear();

    destroyParticipantResources();
    initialized_ = false;
    LOG(info) << "TXDDSNode cleaned up";
}

void TXDDSNode::destroyParticipantResources()
{
    if (participant_ == nullptr) {
        return;
    }

    for (auto &entry : publishers_) {
        if (entry.second != nullptr) {
            participant_->DeletePublisher(entry.second);
        }
    }
    publishers_.clear();

    for (auto &entry : subscribers_) {
        if (entry.second != nullptr) {
            participant_->DeleteSubscriber(entry.second);
        }
    }
    subscribers_.clear();
    BaoSky::rtps::ThreadManager::GetInstance()->DeleteResource("txdds_thread_" + participant_name_);
    BaoSky::rtps::TransportStack::GetInstance()->DeleteConfig("udp");
    BaoSky::dds::DomainParticipantFactory::GetInstance()->DeleteParticipant(participant_);
    participant_ = nullptr;
}

} // namespace TxddsWrapper
