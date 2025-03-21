#ifndef DDSREQUESTREPLYCLIENT_H
#define DDSREQUESTREPLYCLIENT_H

#include "DDSRequestReplyCommon.h"
#include "log/logger.h"

namespace request_reply {
using namespace eprosima::fastdds::dds;

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

class DDSRequestReplyClient : public DomainParticipantListener {
 public:
  DDSRequestReplyClient();
  ~DDSRequestReplyClient();

  void on_participant_discovery(
      DomainParticipant* participant, eprosima::fastdds::rtps::ParticipantDiscoveryStatus status, const ParticipantBuiltinTopicData& info,
      bool& should_be_ignored) override;

  void on_publication_matched(DataWriter* writer, const PublicationMatchedStatus& info) override;

  void on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) override;

  void on_data_available(DataReader* reader) override;
  void  run(); 

  void stop();
 private:
  bool create_participant();
  void create_request_entities(const std::string& service_name);
  void create_reply_entities(const std::string& service_name);
  bool send_request(const CalculatorRequestType& request);
  bool is_stopped();
  bool send_requests();
  void wait_for_replies();

 private:
  DomainParticipant* m_participant_;
  Subscriber* m_subscriber_;
  Publisher* m_publisher_;

  DataReader* ReplyReader_;
  DataWriter* RequestWriter_;

  TypeSupport RequestType_;
  TypeSupport ReplyType_;

  Topic* Replytopic_;
  Topic* RequestTopic_;

  std::mutex mtx_;
  std::atomic<bool> stop_;
  std::condition_variable cv_;
  ContentFilteredTopic* reply_cf_topic_;
  std::string reply_topic_filter_expression_;
  std::vector<std::string> reply_topic_filter_parameters_;
  std::pair<std::int16_t, std::int16_t> request_input_;
  RemoteServerMatchedStatus server_matched_status_;
  std::map<eprosima::fastdds::rtps::SampleIdentity, bool> requests_status_;

  struct Request {
    SampleInfo info;
    std::shared_ptr<CalculatorRequestType> request;
  };

  std::queue<Request> requests_;

  std::thread reply_thread_;
};
}  // namespace request_reply

#endif