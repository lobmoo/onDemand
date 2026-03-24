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

namespace
{
void ensure_transport_resources(const std::string &participant_name)
{
    BaoSky::rtps::ThreadConfig thread_config;
    thread_config.mConfigName = "txdds_thread_" + participant_name;
    thread_config.mThreadName = "txdds_evt_" + participant_name;
    BaoSky::rtps::ThreadManager::GetInstance()->AddResource(thread_config);

    auto udp_config = std::make_shared<BaoSky::rtps::UDPTransportConfig>();
    udp_config->mKind = BaoSky::rtps::eTransportKind::UDPTransportKind;
    udp_config->mLocalIP.push_back("0.0.0.0");
    udp_config->mConfigName = "udp";
    udp_config->mThreadConfigName = thread_config.mConfigName;
    udp_config->mRecvBufferSize = 16 * 1024 * 1024;
    udp_config->mSendBufferSize = 16 * 1024 * 1024;
    BaoSky::rtps::TransportStack::GetInstance()->AddConfig(udp_config);
}
} // namespace

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

TXDDSNode::TXDDSNode(int domainId,
                     const std::string &participant_name,
                     const ParticipantQoSBuilder &participant_qos,
                     ParticipantListener *listener)
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
        ensure_transport_resources(participant_name);

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

        BaoSky::dds::PublisherQos publisher_qos;
        publisher_ = participant_->CreatePublisher(publisher_qos);
        if (publisher_ == nullptr) {
            LOG(error) << "Failed to create TXDDS Publisher";
            destroyParticipantResources();
            return false;
        }

        BaoSky::dds::SubscriberQos subscriber_qos;
        subscriber_ = participant_->CreateSubscriber(subscriber_qos);
        if (subscriber_ == nullptr) {
            LOG(error) << "Failed to create TXDDS Subscriber";
            destroyParticipantResources();
            return false;
        }

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

    if (publisher_ != nullptr) {
        participant_->DeletePublisher(publisher_);
        publisher_ = nullptr;
    }
    if (subscriber_ != nullptr) {
        participant_->DeleteSubscriber(subscriber_);
        subscriber_ = nullptr;
    }

    BaoSky::dds::DomainParticipantFactory::GetInstance()->DeleteParticipant(participant_);
    participant_ = nullptr;
}

} // namespace TxddsWrapper