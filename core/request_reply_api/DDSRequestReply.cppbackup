#include "DDSRequestReply.h"

#include <chrono>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/rtps/common/GuidPrefix_t.hpp>
#include <fastdds/rtps/common/InstanceHandle.hpp>
#include <fastdds/rtps/common/SequenceNumber.hpp>
#include <fastdds/rtps/common/WriteParams.hpp>
#include <fastdds/rtps/participant/ParticipantDiscoveryInfo.hpp>

#include "fastdds_wrapper/DDSParticipantListener.h"
#include "log/logger.h"
namespace request_reply {

DDSRequestReplyNode::DDSRequestReplyNode() {}

DDSRequestReplyNode::~DDSRequestReplyNode() {}

void processDataCallBackServer(const std::string& topic_name, std::shared_ptr<CalculatorRequestType> data) {
  
  LOG(info) << "recv message [" << topic_name << "]: " << data->client_id();
}

void processDataCallBackClient(const std::string& topic_name, std::shared_ptr<CalculatorRequestType> data) {
  LOG(info) << "recv message [" << topic_name << "]: " << data->client_id();
}

bool DDSRequestReplyNode::DDSClient() {
  server_ = std::make_shared<DataNode>(1, "DSF_CMD_CTRL");
  server_->registerTopicType<CalculatorRequestTypePubSubType>("request");
  server_->registerTopicType<CalculatorRequestTypePubSubType>("reply");
  RequestWriter_ = server_->createDataWriter<CalculatorRequestType>("request");
  ReplyReader_ = server_->createDataReader<CalculatorRequestType>("reply", processDataCallBackClient);

  return true;
}

bool DDSRequestReplyNode::DDSServive() {
  client_ = std::make_shared<DataNode>(1, "DSF_CMD_CTRL");
  server_->registerTopicType<CalculatorRequestTypePubSubType>("request");
  server_->registerTopicType<CalculatorRequestTypePubSubType>("reply");
  ReplyWriter_ = server_->createDataWriter<CalculatorRequestType>("reply");
  RequestReader_ = server_->createDataReader<CalculatorRequestType>("request", processDataCallBackServer);
  return true;
}

void DDSRequestReplyNode::on_participant_discovery(
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

void DDSRequestReplyNode::on_publication_matched(DataWriter* writer, const PublicationMatchedStatus& info) {
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

void DDSRequestReplyNode::on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) {
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


}  // namespace request_reply