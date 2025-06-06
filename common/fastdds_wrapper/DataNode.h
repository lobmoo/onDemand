/**
 * @file DataNode.h
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
#ifndef DATANODE_H
#define DATANODE_H

#include <memory>
#include <stdexcept>

#include "DDSParticipantManager.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include "log/logger.h"

class DataNode : public DDSParticipantManager
{
    using ParticipantQosConfigurator = std::function<ParticipantQosHandler()>;

public:
    /**
   * @brief 创建数据通信节点
   * @param  domainId         domainId
   * @param  participant_name  participant_name
   * @param  qos_configurator participant QOS 回调
   */
    DataNode(int domainId, const std::string &participant_name,
             ParticipantQosConfigurator qos_configurator = nullptr,
             DDSParticipantListener *listener = nullptr)
        : DDSParticipantManager(domainId), qos_configurator_(qos_configurator)
    {
        initDomainParticipant(participant_name, listener);
    }

    /**
   * @brief 依照配置文件创建数据通信节点
   * @param  qosXmlConfig     配置文件路径
   */
    DataNode(const std::string &qosXmlConfig, DDSParticipantListener *listener = nullptr)
        : DDSParticipantManager()
    {
        initDomainParticipantForXml(qosXmlConfig, listener);
    }

    ~DataNode() override {}

private:
    mutable std::mutex topicMutex_;
    std::unordered_map<std::string, std::function<eprosima::fastdds::dds::TopicDataType *()>>
        topicTypeFactory_;
    ParticipantQosConfigurator qos_configurator_;

protected:
    ParticipantQosHandler createParticipantQos(const std::string &participant_name) override
    {
        if (nullptr != qos_configurator_) {
            return qos_configurator_();
        } else {
            ParticipantQosHandler handler(participant_name);
            return handler;
        }
    }

public:
    template <typename T>
    std::shared_ptr<DDSTopicDataWriter<T>>
    createDataWriter(const std::string topicName,
                     eprosima::fastdds::dds::DataWriterQos *dataWriterQos = nullptr);

    template <typename T>
    std::shared_ptr<DDSTopicDataReader<T>>
    createDataReader(const std::string topicName,
                     std::function<void(const std::string &, std::shared_ptr<T>)> callback,
                     const eprosima::fastdds::dds::DataReaderQos *dataReaderQos = nullptr);

    template <typename T>
    void registerTopicType(const std::string &topicName)
    {
        addTopicDataTypeCreator(topicName, []() { return new T(); });
    }
};

/**
 * @brief  创建数据写入者
 * @param  topicName        topicName
 * @return DDSTopicDataWriter<T>*
 */
template <typename T>
std::shared_ptr<DDSTopicDataWriter<T>>
DataNode::createDataWriter(const std::string topicName,
                           eprosima::fastdds::dds::DataWriterQos *dataWriterQos)
{
    return DDSParticipantManager::createDataWriter<T>(topicName, dataWriterQos);
}

/**
 * @brief 创建数据读取者
 * @param  topicName        topicName
 * @param  callback         callback 数据接收回调
 * @return DDSTopicDataReader<T>*
 */
template <typename T>
std::shared_ptr<DDSTopicDataReader<T>>
DataNode::createDataReader(const std::string topicName,
                           std::function<void(const std::string &, std::shared_ptr<T>)> callback,
                           const eprosima::fastdds::dds::DataReaderQos *dataReaderQos)
{
    return DDSParticipantManager::createDataReader<T>(topicName, callback, dataReaderQos);
}

#endif
