#include "DDSRequestReplyClient.h"

#include <chrono>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/rtps/common/WriteParams.hpp>

#include "log/logger.h"

namespace request_reply {
using namespace eprosima::fastdds::dds;

DDSRequestReplyClient::DDSRequestReplyClient(const std::string& service_name)
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
      reply_topic_filter_expression_(""),
      stop_(false) {
  create_participant();
  create_request_entities(service_name);
  create_reply_entities(service_name);
  LOG(info) << "ClientApp Client initialized with ID: " << m_participant_->guid().guidPrefix;
}

DDSRequestReplyClient::~DDSRequestReplyClient() {
  if (nullptr != m_participant_) {
    m_participant_->delete_contained_entities();
    DomainParticipantFactory::get_shared_instance()->delete_participant(m_participant_);
  }

  server_matched_status_.clear();
  reply_topic_filter_parameters_.clear();
}

bool DDSRequestReplyClient::create_participant() {
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

void DDSRequestReplyClient::create_request_entities(const std::string& service_name) {
  RequestTopic_ = create_topic<CalculatorRequestTypePubSubType>("rq/" + service_name, m_participant_, RequestType_);

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

void DDSRequestReplyClient::create_reply_entities(const std::string& service_name) {
  Replytopic_ = create_topic<CalculatorReplyTypePubSubType>("rr/" + service_name, m_participant_, ReplyType_);

  reply_topic_filter_expression_ = "client_id = '" + TypeConverter::to_string(m_participant_->guid().guidPrefix) + "'";
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

bool DDSRequestReplyClient::send_requests() {
  CalculatorRequestType request;

  request.client_id(TypeConverter::to_string(m_participant_->guid().guidPrefix));
  request.x(request_input_.first);
  request.y(request_input_.second);

  request.operation(CalculatorOperationType::ADDITION);
  bool ret = send_request(request);

  if (ret) {
    request.operation(CalculatorOperationType::SUBTRACTION);
    ret = send_request(request);
  }

  if (ret) {
    request.operation(CalculatorOperationType::MULTIPLICATION);
    ret = send_request(request);
  }

  if (ret) {
    request.operation(CalculatorOperationType::DIVISION);
    ret = send_request(request);
  }

  return ret;
}

bool DDSRequestReplyClient::send_request(const CalculatorRequestType& request) {
  std::lock_guard<std::mutex> lock(mtx_);

  eprosima::fastdds::rtps::WriteParams wparams;

  ReturnCode_t ret = RequestWriter_->write(&request, wparams);

  requests_status_[wparams.sample_identity()] = false;

  LOG(info) << "ClientApp Request sent with ID '" << wparams.sample_identity().sequence_number() << "': '"
            << TypeConverter::to_string(request) << "'";

  return (RETCODE_OK == ret);
}

bool DDSRequestReplyClient::is_stopped() { return stop_.load(); }

void DDSRequestReplyClient::wait_for_replies() {
  std::unique_lock<std::mutex> lock(mtx_);
  cv_.wait(lock, [&]() { return is_stopped(); });
}

void DDSRequestReplyClient::on_participant_discovery(
    DomainParticipant* /* participant */, eprosima::fastdds::rtps::ParticipantDiscoveryStatus status,
    const ParticipantBuiltinTopicData& info, bool& should_be_ignored) {
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

void DDSRequestReplyClient::on_publication_matched(DataWriter* /* writer */, const PublicationMatchedStatus& info) {
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

void DDSRequestReplyClient::on_subscription_matched(DataReader* /* reader */, const SubscriptionMatchedStatus& info) {
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

void DDSRequestReplyClient::on_data_available(DataReader* reader) {
  SampleInfo info;
  CalculatorReplyType reply;

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

      // Check if all responses have been received
      if (requests_status_.size() == 4) {
        bool all_responses_received = true;

        for (auto status : requests_status_) {
          all_responses_received &= status.second;
        }

        if (all_responses_received) {
          stop();
          break;
        }
      }
    }
  }
}

void DDSRequestReplyClient::stop() {
  stop_.store(true);
  cv_.notify_all();
}

void DDSRequestReplyClient::run() {
  LOG(debug) << "ClientApp Waiting for a server to be available";
  {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [&]() { return server_matched_status_.is_any_server_matched() || is_stopped(); });
  }

  if (!is_stopped()) {
    LOG(debug) << "ClientApp One server is available. Waiting for some time to ensure matching on the server side";

    // TODO(eduponz): This wait should be conditioned to upcoming fully-matched API on the endpoints
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait_for(lock, std::chrono::seconds(1), [&]() { return is_stopped(); });
  }

  if (!is_stopped()) {
    if (!send_requests()) {
      throw std::runtime_error("Failed to send request");
    }

    wait_for_replies();
  }
}

bool DDSRequestReplyClient::send_request_for_wait(const CalculatorRequestType& request) {
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

    LOG(info) << "ClientApp Request sent with ID '" << wparams.sample_identity().sequence_number() << "': '"
              << TypeConverter::to_string(request) << "'";
  }
  wait_for_replies();
  return (RETCODE_OK == ret ? true : false);
}
}  // namespace request_reply