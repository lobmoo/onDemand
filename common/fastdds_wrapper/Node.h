#ifndef NODE_H
#define NODE_H

#include "DDSParticipantManager.h"
#include "ParticipantQosHandler.h"
#include <memory>
#include <mutex>
#include <unordered_set>

class Node : public DDSParticipantManager
{
public:
    // 构造函数 - 带初始化参数
    Node(int domainId, const std::string &participantName, uint16_t listenPort, const std::vector<std::string> &peerLocators);

    // 虚拟析构函数
    virtual ~Node();

    // 创建 DataWriter
    template <typename T>
    DDSTopicDataWriter<T> *createDataWriter(std::string topicName)
    {
        auto dataWriter = DDSParticipantManager::createDataWriter<T>(topicName);
        std::lock_guard<std::mutex> guard(m_topicMutex);
        m_topicRegistry.insert(topicName);
        return dataWriter;
    }

    // 创建 DataReader
    template <typename T>
    DDSTopicDataReader<T> *createDataReader(std::string topicName, std::function<void(const std::string &, std::shared_ptr<T>)> callback)
    {
        auto dataReader = DDSParticipantManager::createDataReader<T>(topicName, callback);
        std::lock_guard<std::mutex> guard(m_topicMutex);
        m_topicRegistry.insert(topicName);
        return dataReader;
    }


    // 获取所有注册的话题
    const std::unordered_set<std::string> &getAllTopics() const;

    // 检查话题是否已注册
    bool hasTopic(const std::string &topicName) const;

private:
    // 重写纯虚函数 createParticipantQos
    virtual ParticipantQosHandler createParticipantQos(const std::string              &participant_name,
                                                       uint16_t                        listen_port,
                                                       const std::vector<std::string> &peer_locators) override;

    // 成员变量
    std::unordered_set<std::string> m_topicRegistry;
    mutable std::mutex m_topicMutex;
};

#endif // NODE_H