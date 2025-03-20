#ifndef REQUESTER_H
#define REQUESTER_H

#include "RequestReplyTypes/Calculator.hpp"
#include "RequestReplyTypes/CalculatorPubSubTypes.hpp"
#include "fastdds_wrapper/DDSDataReaderListener.hpp"
#include "fastdds_wrapper/DDSDataWriterListener.hpp"
#include "fastdds_wrapper/DDSDomainParticipant.h"
#include "fastdds_wrapper/DDSParticipantListener.h"
#include "fastdds_wrapper/DDSParticipantManager.h"
#include "fastdds_wrapper/DDSTopicDataReader.hpp"
#include "fastdds_wrapper/DDSTopicDataWriter.hpp"


#include "fastdds_wrapper/DataNode.h"
#include "log/logger.h"

namespace request_reply {
using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

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

class DDSRequestReplyNode : public DDSDataWriterListener,
                            public DDSDataReaderListener<CalculatorRequestType>,
                            public DDSParticipantListener {
 public:
  DDSRequestReplyNode();
  ~DDSRequestReplyNode();
  bool DDSClient();
  bool DDSServive();

 private:
  std::mutex mtx_;

  RemoteServerMatchedStatus server_matched_status_;
  RemoteClientMatchedStatus client_matched_status_;
  std::shared_ptr<DataNode> server_;
  std::shared_ptr<DataNode> client_;
  DDSTopicDataReader<CalculatorRequestType>* ReplyReader_;
  DDSTopicDataReader<CalculatorRequestType>* RequestReader_;

  DDSTopicDataWriter<CalculatorRequestType>* ReplyWriter_;
  DDSTopicDataWriter<CalculatorRequestType>* RequestWriter_;

 protected:
  void on_participant_discovery(
      DomainParticipant* participant, eprosima::fastdds::rtps::ParticipantDiscoveryStatus reason,
      const ParticipantBuiltinTopicData& info, bool& should_be_ignored) override;

  void on_publication_matched(DataWriter* writer, const PublicationMatchedStatus& info) override;

  void on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) override;

};
}  // namespace request_reply

#endif