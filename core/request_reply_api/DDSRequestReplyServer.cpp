#include "DDSRequestReplyServer.h"

#include <chrono>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/rtps/common/WriteParams.hpp>

#include "log/logger.h"

namespace request_reply {
using namespace eprosima::fastdds::dds;

DDSRequestReplyServerNode::DDSRequestReplyServerNode() {}

DDSRequestReplyServerNode::~DDSRequestReplyServerNode() {}

void processDataCallBackServer(const std::string& topic_name, std::shared_ptr<CalculatorRequestType> data) {
  LOG(info) << "recv message [" << topic_name << "]: " << data->client_id();
}

void processDataCallBackClient(const std::string& topic_name, std::shared_ptr<CalculatorRequestType> data) {
  LOG(info) << "recv message [" << topic_name << "]: " << data->client_id();
}

bool DDSRequestReplyServerNode::create_participant() {
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

  participant_qos.user_data().data_vec().push_back(static_cast<uint8_t>(EntityKind::SERVER));

  m_participant_ = factory->create_participant(participant_qos.domainId(), participant_qos, this, participant_mask);

  if (nullptr == m_participant_) {
    throw std::runtime_error("Participant initialization failed");
  }
  return true;
}

void DDSRequestReplyServerNode::create_request_entities(const std::string& service_name) {
  RequestTopic_ = create_topic<CalculatorReplyTypePubSubType>(service_name, m_participant_, RequestType_);

  SubscriberQos sub_qos = SUBSCRIBER_QOS_DEFAULT;
  if (RETCODE_OK != m_participant_->get_default_subscriber_qos(sub_qos)) {
    throw std::runtime_error("Failed to get default subscriber qos");
  }
  m_subscriber_ = m_participant_->create_subscriber(sub_qos, nullptr, StatusMask::none());
  if (nullptr == m_subscriber_) {
    throw std::runtime_error("Subscriber initialization failed");
  }
  DataReaderQos reader_qos = DATAREADER_QOS_DEFAULT;
  if (RETCODE_OK != m_subscriber_->get_default_datareader_qos(reader_qos)) {
    throw std::runtime_error("Failed to get default datareader qos");
  }
  RequestReader_ = m_subscriber_->create_datareader(RequestTopic_, reader_qos, nullptr, StatusMask::none());

  if (nullptr == RequestReader_) {
    throw std::runtime_error("Request reader initialization failed");
  }
}

void DDSRequestReplyServerNode::create_reply_entities(const std::string& service_name) {
  Replytopic_ = create_topic<CalculatorReplyTypePubSubType>(service_name, m_participant_, ReplyType_);

  PublisherQos pub_qos = PUBLISHER_QOS_DEFAULT;

  if (RETCODE_OK != m_participant_->get_default_publisher_qos(pub_qos)) {
    throw std::runtime_error("Failed to get default publisher qos");
  }

  m_publisher_ = m_participant_->create_publisher(pub_qos, nullptr, StatusMask::none());

  if (nullptr == m_publisher_) {
    throw std::runtime_error("Publisher initialization failed");
  }

  // Create the reply writer
  DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;

  if (RETCODE_OK != m_publisher_->get_default_datawriter_qos(writer_qos)) {
    throw std::runtime_error("Failed to get default datawriter qos");
  }

  ReplyWriter_ = m_publisher_->create_datawriter(Replytopic_, writer_qos, nullptr, StatusMask::none());

  if (nullptr == ReplyWriter_) {
    throw std::runtime_error("Reply writer initialization failed");
  }
}

bool DDSRequestReplyServerNode::is_stopped() { return stop_.load(); }

void DDSRequestReplyServerNode::reply_routine() {
  while (!is_stopped()) {
    std::unique_lock<std::mutex> lock(mtx_);

    cv_.wait(lock, [this]() -> bool { return requests_.size() > 0 || is_stopped(); });

    if (!is_stopped()) {
      Request request = requests_.front();
      requests_.pop();

      eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix =
          eprosima::fastdds::rtps::iHandle2GUID(request.info.publication_handle).guidPrefix;
      LOG(info) << "ServerApp Processing request from client " << client_guid_prefix;

      if (!client_matched_status_.is_fully_unmatched(client_guid_prefix)) {
        LOG(info) << "ServerApp Ignoring request from already gone client " << client_guid_prefix;
        continue;
      }

      if (!client_matched_status_.is_matched(client_guid_prefix)) {
        LOG(debug) << "ServerApp Client " << client_guid_prefix << " not fully matched, saving request for later";
        requests_.push(request);
        continue;
      }

      std::int32_t result;

      if (!calculate(*request.request, result)) {
        LOG(error) << "ServerApp Failed to calculate result for request from client " << client_guid_prefix;
        continue;
      }

      // Prepare the reply
      CalculatorReplyType reply;
      reply.client_id(request.request->client_id());
      reply.result(result);

      // Prepare the WriteParams to link the reply to the request
      eprosima::fastdds::rtps::WriteParams write_params;
      eprosima::fastdds::rtps::SequenceNumber_t request_id = request.info.sample_identity.sequence_number();
      write_params.related_sample_identity().writer_guid(request.info.sample_identity.writer_guid());
      write_params.related_sample_identity().sequence_number(request_id);

      if (RETCODE_OK != ReplyWriter_->write(&reply, write_params)) {
        // In case of failure, save the request for a later retry
        LOG(error) << "ServerApp Failed to send reply to request with ID '" << request_id << "' to client "
                   << client_guid_prefix;
        requests_.push(request);
      } else {
        LOG(error) << "ServerApp Reply to request with ID '" << request_id << "' sent to client " << client_guid_prefix;
      }
    }
  }
}

bool DDSRequestReplyServerNode::calculate(const CalculatorRequestType& request, std::int32_t& result) {
  bool success = true;

  switch (request.operation()) {
    case CalculatorOperationType::ADDITION: {
      result = request.x() + request.y();
      break;
    }
    case CalculatorOperationType::SUBTRACTION: {
      result = request.x() - request.y();
      break;
    }
    case CalculatorOperationType::MULTIPLICATION: {
      result = request.x() * request.y();
      break;
    }
    case CalculatorOperationType::DIVISION: {
      if (0 == request.y()) {
        LOG(error) << "Division by zero request received";
        success = false;
      } else {
        result = request.x() / request.y();
      }
      break;
    }
    default: {
      LOG(error) << "ServerApp", "Unknown operation received";
      success = false;
      break;
    }
  }

  return success;
}

void DDSRequestReplyServerNode::on_participant_discovery(
    DomainParticipant* participant, eprosima::fastdds::rtps::ParticipantDiscoveryStatus reason,
    const ParticipantBuiltinTopicData& info, bool& should_be_ignored) {
  std::lock_guard<std::mutex> lock(mtx_);
  should_be_ignored = false;
  eprosima::fastdds::rtps::GuidPrefix_t remote_participant_guid_prefix = info.guid.guidPrefix;
  std::string status_str = TypeConverter::to_string(reason);
  if (info.user_data.data_vec().size() != 1) {
    should_be_ignored = true;
    LOG(debug) << "ServerApp Ignoring participant with invalid user data: " << remote_participant_guid_prefix;
  }

  if (!should_be_ignored) {
    if (reason == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT) {
      LOG(info) << "ServerApp" << " " << status_str << ": " << remote_participant_guid_prefix;
    } else if (
        reason == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::REMOVED_PARTICIPANT ||
        reason == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::DROPPED_PARTICIPANT) {
      client_matched_status_.match_reply_reader(remote_participant_guid_prefix, false);
      client_matched_status_.match_request_writer(remote_participant_guid_prefix, false);

      LOG(info) << "ServerApp" << " " << status_str << ": " << remote_participant_guid_prefix;
    }
  }
}

void DDSRequestReplyServerNode::on_publication_matched(DataWriter* writer, const PublicationMatchedStatus& info) {
  std::lock_guard<std::mutex> lock(mtx_);
  eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix =
      eprosima::fastdds::rtps::iHandle2GUID(info.last_subscription_handle).guidPrefix;

  if (info.current_count_change == 1) {
    LOG(debug) << "ServerApp  Remote reply reader matched with client " << client_guid_prefix;
    client_matched_status_.match_reply_reader(client_guid_prefix, true);
  } else if (info.current_count_change == -1) {
    LOG(debug) << "ServerApp  Remote reply reader unmatched from client " << client_guid_prefix;
    client_matched_status_.match_reply_reader(client_guid_prefix, false);

    // Remove old replies since no one is waiting for them
    if (client_matched_status_.no_client_matched()) {
      std::size_t removed;
      ReplyWriter_->clear_history(&removed);
    }
  } else {
    LOG(error)
        << "ServerApp info.current_count_change is not a valid value for PublicationMatchedStatus current count change";
  }
};

void DDSRequestReplyServerNode::on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) {
  std::lock_guard<std::mutex> lock(mtx_);

  eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix =
      eprosima::fastdds::rtps::iHandle2GUID(info.last_publication_handle).guidPrefix;

  if (info.current_count_change == 1) {
    LOG(debug) << "ServerApp Remote request writer matched with client " << client_guid_prefix;
    client_matched_status_.match_request_writer(client_guid_prefix, true);
  } else if (info.current_count_change == -1) {
    LOG(debug) << "ServerApp Remote request writer unmatched from client " << client_guid_prefix;
    client_matched_status_.match_request_writer(client_guid_prefix, false);

    // Remove old replies since no one is waiting for them
    if (client_matched_status_.no_client_matched()) {
      std::size_t removed;
      ReplyWriter_->clear_history(&removed);
    }
  } else {
    LOG(error) << "ServerApp" << info.current_count_change
               << " is not a valid value for SubscriptionMatchedStatus current count change";
  }
};

void DDSRequestReplyServerNode::on_data_available(DataReader* reader) {
  SampleInfo info;
  auto request = std::make_shared<CalculatorRequestType>();

  while ((!is_stopped()) && (RETCODE_OK == reader->take_next_sample(request.get(), &info))) {
    if ((info.instance_state == ALIVE_INSTANCE_STATE) && info.valid_data) {
      eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix = eprosima::fastdds::rtps::iHandle2GUID(info.publication_handle).guidPrefix;
      eprosima::fastdds::rtps::SequenceNumber_t request_id = info.sample_identity.sequence_number();

      LOG(info) <<  "ServerApp Request with ID '" << request_id << "' received from client " << client_guid_prefix;
      {
        std::lock_guard<std::mutex> lock(mtx_);
        requests_.push({info, request});
        cv_.notify_all();
      }
    }
  }
}

}  // namespace request_reply