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

template <typename T_RequestSubPubType, typename T_ReplySubPubType, typename T_RequestType, typename T_ReplyType>
class DDSRequestReplyClient : public DomainParticipantListener {
 public:
  DDSRequestReplyClient(
      const std::string& service_name, std::function<void(const T_ReplyType& reply, const SampleInfo& info)> callback)
      : m_participant_(nullptr),
        request_input_({10, 10}),
        RequestType_(nullptr),
        ReplyType_(nullptr),
        m_subscriber_(nullptr),
        ReplyReader_(nullptr),
        RequestTopic_(nullptr),
        Replytopic_(nullptr),
        m_publisher_(nullptr),
        reply_cf_topic_(nullptr),
        RequestWriter_(nullptr),
        reply_callback_(callback),
        reply_topic_filter_expression_(""),
        stop_(false) 
    {
      create_participant();
      create_request_entities(service_name);
      create_reply_entities(service_name);
      LOG(info) << "ClientApp Client initialized with ID: " << m_participant_->guid().guidPrefix;
    }

    ~DDSRequestReplyClient() {
      if (nullptr != m_participant_) {
        m_participant_->delete_contained_entities();
        DomainParticipantFactory::get_shared_instance()->delete_participant(m_participant_);
      }

      server_matched_status_.clear();
      reply_topic_filter_parameters_.clear();
    }

    bool send_request_for_wait(const T_RequestType& request) {
      ReturnCode_t ret = RETCODE_ERROR;
      {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&]() { return server_matched_status_.is_any_server_matched() || is_stopped(); });
      }

      if (!is_stopped()) {
        LOG(debug) << "ClientApp One server is available. Waiting for some time to ensure matching on the server side";
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, std::chrono::seconds(1), [&]() { return is_stopped(); });
      }
      {
        std::lock_guard<std::mutex> lock(mtx_);

        eprosima::fastdds::rtps::WriteParams wparams;

        ReturnCode_t ret = RequestWriter_->write(&request, wparams);

        requests_status_[wparams.sample_identity()] = false;

      }
      wait_for_replies();
      return (RETCODE_OK == ret ? true : false);
    }

   private:
    bool create_participant() {
      auto factory = DomainParticipantFactory::get_instance();
      if (nullptr == factory) {
        throw std::runtime_error("Failed to get participant factory instance");
      }
      StatusMask participant_mask = StatusMask::none();
      participant_mask << StatusMask::publication_matched();
      participant_mask << StatusMask::subscription_matched();
      participant_mask << StatusMask::data_available();

      DomainParticipantExtendedQos participant_qos;
      factory->get_participant_extended_qos_from_default_profile(participant_qos);

      participant_qos.user_data().data_vec().push_back(static_cast<uint8_t>(EntityKind::CLIENT));

      m_participant_ = factory->create_participant(participant_qos.domainId(), participant_qos, this, participant_mask);

      if (nullptr == m_participant_) {
        throw std::runtime_error("Participant initialization failed");
      }
      return true;
    }

    void create_request_entities(const std::string& service_name) {
      RequestTopic_ = create_topic<T_RequestSubPubType>("rq/" + service_name, m_participant_, RequestType_);

      PublisherQos pub_qos = PUBLISHER_QOS_DEFAULT;
      if (RETCODE_OK != m_participant_->get_default_publisher_qos(pub_qos)) {
        throw std::runtime_error("Failed to get default publisher qos");
      }

      m_publisher_ = m_participant_->create_publisher(pub_qos, nullptr, StatusMask::none());
      if (nullptr == m_publisher_) {
        throw std::runtime_error("Publisher initialization failed");
      }

      DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;
      if (RETCODE_OK != m_publisher_->get_default_datawriter_qos(writer_qos)) {
        throw std::runtime_error("Failed to get default datawriter qos");
      }

      RequestWriter_ = m_publisher_->create_datawriter(RequestTopic_, writer_qos, nullptr, StatusMask::none());
      if (nullptr == RequestWriter_) {
        throw std::runtime_error("Request reader initialization failed");
      }
    }

    void create_reply_entities(const std::string& service_name) {
      Replytopic_ = create_topic<T_ReplySubPubType>("rr/" + service_name, m_participant_, ReplyType_);

      reply_topic_filter_expression_ =
          "client_id = '" + TypeConverter::to_string(m_participant_->guid().guidPrefix) + "'";
      reply_cf_topic_ = m_participant_->create_contentfilteredtopic(
          "rr/" + service_name + "_cft", Replytopic_, reply_topic_filter_expression_, reply_topic_filter_parameters_);
      if (nullptr == reply_cf_topic_) {
        throw std::runtime_error("Failed to create CFT");
      }

      SubscriberQos sub_qos = SUBSCRIBER_QOS_DEFAULT;

      if (RETCODE_OK != m_participant_->get_default_subscriber_qos(sub_qos)) {
        throw std::runtime_error("Failed to get default subscriber qos");
      }
      m_subscriber_ = m_participant_->create_subscriber(sub_qos, nullptr, StatusMask::none());
      if (nullptr == m_subscriber_) {
        throw std::runtime_error("Publisher initialization failed");
      }

      DataReaderQos reader_qos = DATAREADER_QOS_DEFAULT;
      if (RETCODE_OK != m_subscriber_->get_default_datareader_qos(reader_qos)) {
        throw std::runtime_error("Failed to get default datareader qos");
      }

      ReplyReader_ = m_subscriber_->create_datareader(Replytopic_, reader_qos, nullptr, StatusMask::none());
      if (nullptr == ReplyReader_) {
        throw std::runtime_error("Reply writer initialization failed");
      }
    }

    bool is_stopped() { return stop_.load(); }

    void stop() {
      stop_.store(true);
      cv_.notify_all();
    }

    void wait_for_replies() {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [&]() { return is_stopped(); });
    }

   protected:
    void on_participant_discovery(
        DomainParticipant* /* participant */, eprosima::fastdds::rtps::ParticipantDiscoveryStatus status,
        const ParticipantBuiltinTopicData& info, bool& should_be_ignored) override {
      std::lock_guard<std::mutex> lock(mtx_);

      should_be_ignored = false;

      eprosima::fastdds::rtps::GuidPrefix_t remote_participant_guid_prefix = info.guid.guidPrefix;
      std::string status_str = TypeConverter::to_string(status);

      if (info.user_data.data_vec().size() != 1) {
        should_be_ignored = true;
        LOG(info) << "ClientApp Ignoring participant with invalid user data " << remote_participant_guid_prefix;
      }

      if (!should_be_ignored) {
        EntityKind entity_kind = static_cast<EntityKind>(info.user_data.data_vec()[0]);
        if (EntityKind::SERVER != entity_kind) {
          should_be_ignored = true;
          LOG(debug) << "ClientApp Ignoring status_str" << static_cast<uint8_t>(entity_kind) << ": "
                     << remote_participant_guid_prefix;
        }
      }

      if (!should_be_ignored) {
        if (status == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT) {
          LOG(debug) << "ClientApp server_str" << "server" << ": " << remote_participant_guid_prefix;
        } else if (
            status == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::REMOVED_PARTICIPANT ||
            status == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::DROPPED_PARTICIPANT) {
          LOG(debug) << "ClientApp server_str" << "server" << ": " << remote_participant_guid_prefix;
        }
      }
    }

    void on_publication_matched(DataWriter* /* writer */, const PublicationMatchedStatus& info) override {
      std::lock_guard<std::mutex> lock(mtx_);

      eprosima::fastdds::rtps::GuidPrefix_t server_guid_prefix =
          eprosima::fastdds::rtps::iHandle2GUID(info.last_subscription_handle).guidPrefix;

      if (info.current_count_change == 1) {
        LOG(debug) << "ClientApp Remote request reader matched.";

        server_matched_status_.match_request_reader(server_guid_prefix, true);
      } else if (info.current_count_change == -1) {
        LOG(debug) << "ClientApp Remote request reader unmatched.";
        server_matched_status_.match_request_reader(server_guid_prefix, false);
      } else {
        LOG(error) << "ClientApp info.current_count_change is not a valid value for SubscriptionMatchedStatus current "
                      "count change";
      }
      cv_.notify_all();
    }

    void on_subscription_matched(DataReader* /* reader */, const SubscriptionMatchedStatus& info) override {
      std::lock_guard<std::mutex> lock(mtx_);

      eprosima::fastdds::rtps::GuidPrefix_t server_guid_prefix =
          eprosima::fastdds::rtps::iHandle2GUID(info.last_publication_handle).guidPrefix;

      if (info.current_count_change == 1) {
        LOG(debug) << "ClientApp Remote reply writer matched.";
        server_matched_status_.match_reply_writer(server_guid_prefix, true);
      } else if (info.current_count_change == -1) {
        LOG(debug) << "ClientApp Remote reply writer unmatched.";
        server_matched_status_.match_reply_writer(server_guid_prefix, false);
      } else {
        LOG(error) << "ClientApp info.current_count_change is not a valid value for SubscriptionMatchedStatus current "
                      "count change";
      }
      cv_.notify_all();
    }

    void on_data_available(DataReader * reader) override {
      SampleInfo info;
      T_ReplyType reply;

      while ((!is_stopped()) && (RETCODE_OK == reader->take_next_sample(&reply, &info))) {
        if ((info.instance_state == ALIVE_INSTANCE_STATE) && info.valid_data) {
          std::lock_guard<std::mutex> lock(mtx_);

          eprosima::fastdds::rtps::GuidPrefix_t server_guid_prefix =
              eprosima::fastdds::rtps::iHandle2GUID(info.publication_handle).guidPrefix;

          auto request_status = requests_status_.find(info.related_sample_identity);

          if (requests_status_.end() != request_status) {
            if (!request_status->second) {
              request_status->second = true;
              LOG(info) << "ClientApp Reply received from server " << server_guid_prefix << " to request with ID '"
                        << request_status->first.sequence_number() << "' with result: '" << reply.result() << "'";
              if (reply_callback_) {
                reply_callback_(reply, info);
              }
            } else {
              LOG(debug) << "ClientApp Duplicate reply received from server " << server_guid_prefix
                         << " to request with ID '" << request_status->first.sequence_number() << "' with result: '"
                         << reply.result() << "'";
              continue;
            }
          } else {
            LOG(error) << "ClientApp  Reply received from server " << server_guid_prefix << " with unknown request ID '"
                       << info.related_sample_identity.sequence_number() << "'";
            continue;
          }

          bool all_responses_received = true;
          for (auto& status : requests_status_) {
            all_responses_received &= status.second;
          }

          if (all_responses_received) {
            stop();
            break;
          }
        }
      }
    }

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
      std::shared_ptr<T_RequestType> request;
    };

    std::queue<Request> requests_;
    std::function<void(const T_ReplyType& reply, const SampleInfo& info)> reply_callback_;
    std::thread reply_thread_;
  };
}// namespace request_reply

#endif