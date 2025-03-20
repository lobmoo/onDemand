#include "DDSRequestReplyClient.h"

#include <chrono>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/rtps/common/WriteParams.hpp>

#include "log/logger.h"

namespace request_reply {
using namespace eprosima::fastdds::dds;

DDSRequestReplyClient::DDSRequestReplyClient() {}

DDSRequestReplyClient::~DDSRequestReplyClient() {}

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
  RequestTopic_ = create_topic<CalculatorReplyTypePubSubType>(service_name, m_participant_, RequestType_);

  SubscriberQos sub_qos = SUBSCRIBER_QOS_DEFAULT;
  if (RETCODE_OK != m_participant_->get_default_subscriber_qos(sub_qos)) {
    throw std::runtime_error("Failed to get default subscriber qos");
  }

  PublisherQos pub_qos = PUBLISHER_QOS_DEFAULT;
  m_publisher_ = m_participant_->create_publisher(pub_qos, nullptr, StatusMask::none());
  if (nullptr == m_publisher_) {
    throw std::runtime_error("Publisher initialization failed");
  }

  DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;
  RequestWriter_ = m_publisher_->create_datawriter(RequestTopic_, writer_qos, nullptr, StatusMask::none());
  if (nullptr == RequestWriter_) {
    throw std::runtime_error("Request reader initialization failed");
  }
}

void DDSRequestReplyClient::create_reply_entities(const std::string& service_name) {
  Replytopic_ = create_topic<CalculatorReplyTypePubSubType>(service_name, m_participant_, ReplyType_);

  reply_topic_filter_expression_ = "client_id = '" + TypeConverter::to_string(m_participant_->guid().guidPrefix) + "'";
  reply_cf_topic_ = m_participant_->create_contentfilteredtopic(
      service_name + "_cft", Replytopic_, reply_topic_filter_expression_, reply_topic_filter_parameters_);
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

  ReplyReader_ = m_subscriber_->create_datareader(reply_cf_topic_, reader_qos, nullptr, StatusMask::none());
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

}  // namespace request_reply