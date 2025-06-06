/**
 * @file DDSRequestReplyCommon.h
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
#ifndef DDSREQUESTREPLYCOMMON_H
#define DDSREQUESTREPLYCOMMON_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/common/WriteParams.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#define SERVER_NAME "calculator_service"

using namespace eprosima::fastdds::dds;
namespace request_reply
{

struct TypeConverter {

    static std::string to_string(const eprosima::fastdds::rtps::GuidPrefix_t &guid_prefix)
    {
        std::ostringstream client_id;
        client_id << guid_prefix;
        return client_id.str();
    }

    static std::string to_string(const eprosima::fastdds::rtps::ParticipantDiscoveryStatus &info)
    {
        std::string info_str = "Unknown";

        switch (info) {
            case eprosima::fastdds::rtps::ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT:
                info_str = "discovered";
                break;
            case eprosima::fastdds::rtps::ParticipantDiscoveryStatus::CHANGED_QOS_PARTICIPANT:
                info_str = "changed QoS";
                break;
            case eprosima::fastdds::rtps::ParticipantDiscoveryStatus::REMOVED_PARTICIPANT:
                info_str = "removed";
                break;
            case eprosima::fastdds::rtps::ParticipantDiscoveryStatus::DROPPED_PARTICIPANT:
                info_str = "dropped";
                break;
            case eprosima::fastdds::rtps::ParticipantDiscoveryStatus::IGNORED_PARTICIPANT:
                info_str = "ignored";
                break;
            default:
                break;
        }

        return info_str;
    }
};

enum class EntityKind : std::uint8_t { SERVER, CLIENT, UNDEFINED };

template <typename TypeSupportClass>
Topic *create_topic(const std::string &topic_name, DomainParticipant *participant,
                    TypeSupport &type)
{
    assert(nullptr != participant);
    assert(nullptr == type.get());

    Topic *topic = nullptr;

    // Create the TypeSupport
    type.reset(new TypeSupportClass());

    if (nullptr == type) {
        throw std::runtime_error("Failed to create type");
    }

    // Register the type
    if (RETCODE_OK != type.register_type(participant)) {
        throw std::runtime_error("Failed to register type");
    }

    // Create the topic
    TopicQos topic_qos = TOPIC_QOS_DEFAULT;

    if (RETCODE_OK != participant->get_default_topic_qos(topic_qos)) {
        throw std::runtime_error("Failed to get default topic qos");
    }

    topic = participant->create_topic(topic_name, type.get_type_name(), topic_qos);

    if (nullptr == topic) {
        throw std::runtime_error("Request topic initialization failed");
    }

    return topic;
}

struct Timestamp {
    static std::string now()
    {
        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        // Convert to tm struct for local time
        std::tm tm_now;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif // if defined(_WIN32) || defined(_WIN64)

        // Format date and time
        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%S");

        // Add milliseconds
        auto duration = now.time_since_epoch();
        auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
        oss << '.' << std::setfill('0') << std::setw(3) << milliseconds;

        return oss.str();
    }
};

} // namespace request_reply

#endif // DDSREQUESTREPLYCOMMON_H