#include "DDSRequestReply.h"

#include <chrono>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>

#include "fastdds_wrapper/DDSParticipantListener.h"
#include "fastdds_wrapper/DataNode.h"
#include "log/logger.h"

DDSRequestReplyNode::DDSRequestReplyNode() {}

DDSRequestReplyNode::~DDSRequestReplyNode() {}

void processDataCallBackServer(const std::string &topic_name, std::shared_ptr<CalculatorRequestType> data) {
  LOG(info) << "recv message [" << topic_name << "]: " << data->client_id();
}

void processDataCallBackClient(const std::string &topic_name, std::shared_ptr<CalculatorRequestType> data) {
  LOG(info) << "recv message [" << topic_name << "]: " << data->client_id();
}

bool DDSRequestReplyNode::DDSClient() {
  DataNode node(1, "DSF_CMD_CTRL");
  node.registerTopicType<CalculatorRequestTypePubSubType>("request");
  node.registerTopicType<CalculatorRequestTypePubSubType>("reply");
  auto dataWriter = node.createDataWriter<CalculatorRequestType>("request");
  auto dataReader = node.createDataReader<CalculatorRequestType>("reply", processDataCallBackServer);
  return true;
}

bool DDSRequestReplyNode::DDSServive() {
  DataNode node(1, "DSF_CMD_CTRL");
  node.registerTopicType<CalculatorRequestTypePubSubType>("request");
  node.registerTopicType<CalculatorRequestTypePubSubType>("reply");
  auto dataWriter = node.createDataWriter<CalculatorRequestType>("reply");
  auto dataReader = node.createDataReader<CalculatorRequestType>("request", processDataCallBackClient);
  return true;
}
