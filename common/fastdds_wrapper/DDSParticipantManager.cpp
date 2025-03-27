/**
 * @file DDSParticipantManager.cpp
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-03-25
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-03-25     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#include "DDSParticipantManager.h"

#include <memory>

#include "DDSDomainParticipant.h"
#include "ParticipantQosHandler.h"

DDSParticipantManager::DDSParticipantManager(int domainId)
    : m_domainId(domainId), m_isXmlConfig_(false)
{
}
DDSParticipantManager::DDSParticipantManager() : m_isXmlConfig_(false)
{
}

DDSParticipantManager::~DDSParticipantManager()
{
}

bool DDSParticipantManager::initDomainParticipant(const std::string &participant_name,
                                                  DomainParticipantListener *listener)
{
    if (!m_participant) {
        eprosima::fastdds::dds::DomainParticipantExtendedQos participantQos =
            createParticipantQos(participant_name).getQos();
        m_participant =
            std::make_shared<DDSDomainParticipant>(m_domainId, participantQos, listener);

        if (!m_participant)
            return false;
    }
    return true;
}

bool DDSParticipantManager::initDomainParticipantForXml(const std::string &xmlConfig,
                                                        DomainParticipantListener *listener)
{
    if (!m_participant) {
        m_isXmlConfig_ = true;
        m_participant = std::make_shared<DDSDomainParticipant>(xmlConfig, listener);
        if (!m_participant)
            return false;
    }
    return true;
}

eprosima::fastdds::dds::TopicDataType *
DDSParticipantManager::getTopicDataType(std::string topicName)
{
    if (m_topicTypes.find(topicName) == m_topicTypes.end()) {
        LOG(error) << "Cann't found topic: " << topicName;
        return nullptr;
    }
    return m_topicTypes.at(topicName)();
}

void DDSParticipantManager::addTopicDataTypeCreator(std::string topicName,
                                                    TopicDataTypeCreator creator)
{
    if (m_topicTypes.find(topicName) == m_topicTypes.end())
        m_topicTypes.insert(std::make_pair(topicName, creator));
}