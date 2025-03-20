#ifndef REQUESTER_H
#define REQUESTER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "RequestReplyTypes/Calculator.hpp"
#include "RequestReplyTypes/CalculatorPubSubTypes.hpp"
#include "log/logger.h"

namespace request_reply {
using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

enum class EntityKind : std::uint8_t { SERVER, CLIENT, UNDEFINED };

template <typename TypeSupportClass>
Topic* create_topic(const std::string& topic_name, DomainParticipant* participant, TypeSupport& type) {
  assert(nullptr != participant);
  assert(nullptr == type.get());

  Topic* topic = nullptr;

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
  static std::string now() {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    // Convert to tm struct for local time
    std::tm tm_now;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif  // if defined(_WIN32) || defined(_WIN64)

    // Format date and time
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%S");

    // Add milliseconds
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
    oss << '.' << std::setfill('0') << std::setw(3) << milliseconds;

    return oss.str();
  }
};

struct TypeConverter {
  static std::string to_string(const CalculatorRequestType& request) {
    std::ostringstream request_ss;
    request_ss << request.x() << " " << to_string(request.operation()) << " " << request.y();
    return request_ss.str();
  }

  static std::string to_string(const CalculatorOperationType& operation) {
    std::string operation_str = "Unknown";
    switch (operation) {
      case CalculatorOperationType::ADDITION:
        operation_str = "+";
        break;
      case CalculatorOperationType::SUBTRACTION:
        operation_str = "-";
        break;
      case CalculatorOperationType::MULTIPLICATION:
        operation_str = "*";
        break;
      case CalculatorOperationType::DIVISION:
        operation_str = "/";
        break;
      default:
        break;
    }
    return operation_str;
  }

  static std::string to_string(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix) {
    std::ostringstream client_id;
    client_id << guid_prefix;
    return client_id.str();
  }

  static std::string to_string(const eprosima::fastdds::rtps::ParticipantDiscoveryStatus& info) {
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

class RemoteServerMatchedStatus {
 public:
  void match_request_reader(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix, const bool& status) {
    matched_status_[guid_prefix].set(request_reader_position, status);
  }

  void match_reply_writer(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix, const bool& status) {
    matched_status_[guid_prefix].set(reply_writer_position, status);
  }

  bool is_matched(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix) {
    bool is_server_matched = false;

    auto status = matched_status_.find(guid_prefix);

    if (status != matched_status_.end()) {
      is_server_matched = status->second.all();
    }

    return is_server_matched;
  }

  bool is_any_server_matched() {
    bool any_server_matched = false;
    for (const auto& status : matched_status_) {
      if (status.second.all()) {
        any_server_matched = true;
        break;
      }
    }
    return any_server_matched;
  }

  void clear() { matched_status_.clear(); }

 private:
  std::map<eprosima::fastdds::rtps::GuidPrefix_t, std::bitset<2>> matched_status_;

  static const size_t request_reader_position = 0;

  static const size_t reply_writer_position = 1;
};

class RemoteClientMatchedStatus {
 public:
  void match_request_writer(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix, const bool& status) {
    matched_status_[guid_prefix].set(request_writer_position, status);
  }

  void match_reply_reader(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix, const bool& status) {
    matched_status_[guid_prefix].set(reply_reader_position, status);
  }

  bool is_matched(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix) {
    bool is_client_matched = false;

    auto status = matched_status_.find(guid_prefix);

    if (status != matched_status_.end()) {
      is_client_matched = status->second.all();
    }

    return is_client_matched;
  }

  bool is_fully_unmatched(const eprosima::fastdds::rtps::GuidPrefix_t& guid_prefix) {
    bool is_client_unmatched = false;

    auto status = matched_status_.find(guid_prefix);

    if (status != matched_status_.end()) {
      is_client_unmatched = !status->second.none();
    }

    return is_client_unmatched;
  }

  bool no_client_matched() {
    bool no_client_matched = true;

    for (const auto& status : matched_status_) {
      if (status.second.any()) {
        no_client_matched = false;
        break;
      }
    }

    return no_client_matched;
  }

  void clear() { matched_status_.clear(); }

 private:
  std::map<eprosima::fastdds::rtps::GuidPrefix_t, std::bitset<2>> matched_status_;

  static const size_t request_writer_position = 0;

  static const size_t reply_reader_position = 1;
};

class DDSRequestReplyServerNode : public DomainParticipantListener {
 public:
  DDSRequestReplyServerNode();
  ~DDSRequestReplyServerNode();
  bool DDSServive();

 private:
  bool create_participant();
  void create_request_entities(const std::string& service_name);
  void create_reply_entities(const std::string& service_name);
  bool is_stopped();
  void reply_routine();
  bool calculate(const CalculatorRequestType& request,
    std::int32_t& result);

 private:
  RemoteClientMatchedStatus client_matched_status_;

  DomainParticipant* m_participant_;
  Subscriber* m_subscriber_;
  Publisher* m_publisher_;

  DataReader* ReplyReader_;
  DataReader* RequestReader_;

  DataWriter* ReplyWriter_;
  DataWriter** RequestWriter_;

  TypeSupport RequestType_;
  TypeSupport ReplyType_;

  Topic* Replytopic_;
  Topic* RequestTopic_;

  std::mutex mtx_;
  std::atomic<bool> stop_;
  std::condition_variable cv_;

  struct Request {
    SampleInfo info;
    std::shared_ptr<CalculatorRequestType> request;
  };

  std::queue<Request> requests_;

  std::thread reply_thread_;

 protected:
  void on_participant_discovery(
      DomainParticipant* participant, eprosima::fastdds::rtps::ParticipantDiscoveryStatus status,
      const ParticipantBuiltinTopicData& info, bool& should_be_ignored) override;

  void on_publication_matched(DataWriter* writer, const PublicationMatchedStatus& info) override;

  void on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) override;

  void on_data_available(DataReader* reader) override;
};
}  // namespace request_reply

#endif