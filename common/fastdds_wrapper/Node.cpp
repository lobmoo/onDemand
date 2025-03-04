#include "Node.h"

Node::Node(int domainId, const std::string &participantName, uint16_t listenPort, const std::vector<std::string> &peerLocators)
    : DDSParticipantManager(domainId)
{
    LOG(info) << "Initializing Node with participant name: " << participantName;

    // ³õÊ¼»¯ Participant
    if (!this->initDomainParticipant(participantName, listenPort, peerLocators))
    {
        LOG(error) << "Failed to initialize participant.";
    }
}

Node::~Node()
{
    LOG(info) << "Destroying Node and unregistering all topics.";

}

ParticipantQosHandler Node::createParticipantQos(const std::string &participant_name,
                                                 uint16_t listen_port,
                                                 const std::vector<std::string> &peer_locators)
{
    LOG(info) << "Configuring ParticipantQos for '" << participant_name << "'.";

    ParticipantQosHandler handler(participant_name);
    handler.addTCPV4Transport(listen_port, peer_locators);

    return handler;
}


const std::unordered_set<std::string> &Node::getAllTopics() const
{
    return m_topicRegistry;
}

bool Node::hasTopic(const std::string &topicName) const
{
    std::lock_guard<std::mutex> guard(m_topicMutex);
    return m_topicRegistry.find(topicName) != m_topicRegistry.end();
}