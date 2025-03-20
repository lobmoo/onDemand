#ifndef REQUESTER_H
#define REQUESTER_H

#include "fastdds_wrapper/DDSDomainParticipant.h"
#include "fastdds_wrapper/DDSParticipantManager.h"
#include "fastdds_wrapper/DDSTopicDataReader.hpp"
#include "fastdds_wrapper/DDSTopicDataWriter.hpp"
#include "log/logger.h"
#include "RequestReplyTypes/CalculatorPubSubTypes.hpp"
class DDSRequestReplyNode {
 public:
  DDSRequestReplyNode();
  ~DDSRequestReplyNode();
  bool DDSClient();
  bool DDSServive();

  private:
  

};
#endif