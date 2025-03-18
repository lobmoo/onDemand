#ifndef REQUESTER_H
#define REQUESTER_H

#include <future>
#include <string>

#include "DDSDomainParticipant.h"
#include "DDSTopicDataReader.hpp"
#include "DDSTopicDataWriter.hpp"
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

struct RequestMessage {
  std::string correlationId;
  std::string data;
};

struct ReplyMessage {
  std::string correlationId;
  std::string data;
};

class Requester {
 public:
  Requester(const std::string& requestTopic, const std::string& replyTopic);
  ~Requester();
  std::string sendRequest(const std::string& data);

 private:
  eprosima::fastdds::dds::DomainParticipant* m_participant;
  std::string m_requestTopic;
  std::string m_replyTopic;
  std::shared_ptr<DDSTopicDataWriter<RequestMessage>> m_requestWriter;
  std::shared_ptr<DDSTopicDataReader<ReplyMessage>> m_replyReader;
  std::map<std::string, std::promise<ReplyMessage>> m_pendingRequests;
  std::mutex m_mutex;
  int m_nextId;

  void onReplyReceived(const ReplyMessage& msg);
};

#endif