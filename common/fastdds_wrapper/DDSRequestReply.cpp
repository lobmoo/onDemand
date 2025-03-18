#include "DDSRequestReply.h"

#include <chrono>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
using namespace eprosima::fastdds::dds;

Requester::Requester(const std::string& requestTopic, const std::string& replyTopic)
    : m_participant(nullptr), m_requestTopic(requestTopic), m_replyTopic(replyTopic), m_nextId(0) {
  eprosima::fastdds::dds::StatusMask participant_mask = eprosima::fastdds::dds::StatusMask::none();
  participant_mask << eprosima::fastdds::dds::StatusMask::publication_matched();
  participant_mask << eprosima::fastdds::dds::StatusMask::subscription_matched();
  participant_mask << eprosima::fastdds::dds::StatusMask::data_available();

  eprosima::fastdds::dds::DomainParticipantExtendedQos participant_qos;

  auto factory = DomainParticipantFactory::get_instance();
  factory->get_participant_extended_qos_from_default_profile(participant_qos);

  participant_qos.user_data().data_vec().push_back(static_cast<uint8_t>(0));

  m_participant = factory->create_participant(participant_qos.domainId(), participant_qos, nullptr, participant_mask);
  if (!m_participant) {
     LOG(error) << "Failed to create participant";                                                       
  }

//   m_participant->registerTopic(m_requestTopic, "RequestMessage");
//   m_participant->registerTopic(m_replyTopic, "ReplyMessage");

//   eprosima::fastdds::dds::DataWriterQos writerQos;
//   writerQos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
//   writerQos.reliability().max_blocking_time = eprosima::fastdds::dds::Duration_t(10000000000);
//   m_requestWriter = m_participant->createDataWriter<RequestMessage>(m_requestTopic, writerQos);

//   eprosima::fastdds::dds::DataReaderQos readerQos;
//   readerQos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
//   m_replyReader = m_participant->createDataReader<ReplyMessage>(
//       m_replyTopic, [this](const std::string&, const ReplyMessage& msg) { onReplyReceived(msg); }, readerQos);
}

Requester::~Requester() {}

std::string Requester::sendRequest(const std::string& data) {
  std::string correlationId = std::to_string(m_nextId++);
  RequestMessage request = {correlationId, data};

  m_requestWriter->writeMessage(request);

  auto it = m_pendingRequests.find(correlationId);
  if (it != m_pendingRequests.end()) {
    auto future = it->second.get_future();
    if (future.wait_for(std::chrono::seconds(10)) == std::future_status::ready) {
      auto reply = future.get();
      m_pendingRequests.erase(it);
      return reply.data;
    } else {
      m_pendingRequests.erase(it);
      throw std::runtime_error("Timeout waiting for reply");
    }
  }
  throw std::runtime_error("No reply received for request");
}

void Requester::onReplyReceived(const ReplyMessage& msg) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_pendingRequests.find(msg.correlationId);
  if (it != m_pendingRequests.end()) {
    it->second.set_value(msg);
  }
}