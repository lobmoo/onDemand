/**
 * @file DDSRequestReplyServer.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-03-25
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-03-25     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef DDSREQUESTREPLYSERVER_H
#define DDSREQUESTREPLYSERVER_H

#include "DDSRequestReplyCommon.h"
#include "log/logger.h"
namespace request_reply
{

class RemoteClientMatchedStatus
{
public:
    void match_request_writer(const eprosima::fastdds::rtps::GuidPrefix_t &guid_prefix,
                              const bool &status)
    {
        matched_status_[guid_prefix].set(request_writer_position, status);
    }

    void match_reply_reader(const eprosima::fastdds::rtps::GuidPrefix_t &guid_prefix,
                            const bool &status)
    {
        matched_status_[guid_prefix].set(reply_reader_position, status);
    }

    bool is_matched(const eprosima::fastdds::rtps::GuidPrefix_t &guid_prefix)
    {
        bool is_client_matched = false;

        auto status = matched_status_.find(guid_prefix);

        if (status != matched_status_.end()) {
            is_client_matched = status->second.all();
        }

        return is_client_matched;
    }

    bool is_fully_unmatched(const eprosima::fastdds::rtps::GuidPrefix_t &guid_prefix)
    {
        bool is_client_unmatched = false;

        auto status = matched_status_.find(guid_prefix);

        if (status != matched_status_.end()) {
            is_client_unmatched = !status->second.none();
        }

        return is_client_unmatched;
    }

    bool no_client_matched()
    {
        bool no_client_matched = true;

        for (const auto &status : matched_status_) {
            if (status.second.any()) {
                no_client_matched = false;
                break;
            }
        }

        return no_client_matched;
    }

    void clear() { matched_status_.clear(); }

private:
    std::map<eprosima::fastdds::rtps::GuidPrefix_t, std::bitset<2>> matched_status_;

    static const size_t request_writer_position = 0;

    static const size_t reply_reader_position = 1;
};

template <typename T_RequestSubPubType, typename T_ReplySubPubType, typename T_RequestType,
          typename T_ReplyType>
class DDSRequestReplyServer : public DomainParticipantListener
{
public:
    DDSRequestReplyServer(
        const std::string &service_name,
        std::function<void(const T_RequestType &request, T_ReplyType &reply)> callback)
        : m_participant_(nullptr), RequestType_(nullptr), ReplyType_(nullptr),
          m_subscriber_(nullptr), RequestReader_(nullptr), RequestTopic_(nullptr),
          Replytopic_(nullptr), m_publisher_(nullptr), ReplyWriter_(nullptr),
          request_callback_(callback), stop_(false)
    {
        reply_thread_ = std::thread(&DDSRequestReplyServer::reply_routine, this);
        create_participant();
        create_request_entities(service_name);
        create_reply_entities(service_name);
        LOG(info) << "ServerApp Server initialized with ID: " << m_participant_->guid().guidPrefix;
    }

    ~DDSRequestReplyServer()
    {
        stop();
        if (reply_thread_.joinable()) {
            reply_thread_.join();
        }

        if (nullptr != m_participant_) {
            m_participant_->delete_contained_entities();
            DomainParticipantFactory::get_instance()->delete_participant(m_participant_);
        }

        while (requests_.size() > 0) {
            requests_.pop();
        }
        client_matched_status_.clear();
    }

private:
    bool create_participant()
    {
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

        m_participant_ = factory->create_participant(participant_qos.domainId(), participant_qos,
                                                     this, participant_mask);

        if (nullptr == m_participant_) {
            throw std::runtime_error("Participant initialization failed");
        }
        return true;
    }

    void create_request_entities(const std::string &service_name)
    {
        RequestTopic_ =
            create_topic<T_RequestSubPubType>("rq/" + service_name, m_participant_, RequestType_);

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
        RequestReader_ = m_subscriber_->create_datareader(RequestTopic_, reader_qos, nullptr,
                                                          StatusMask::none());

        if (nullptr == RequestReader_) {
            throw std::runtime_error("Request reader initialization failed");
        }
    }

    void create_reply_entities(const std::string &service_name)
    {
        Replytopic_ =
            create_topic<T_ReplySubPubType>("rr/" + service_name, m_participant_, ReplyType_);

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

        ReplyWriter_ =
            m_publisher_->create_datawriter(Replytopic_, writer_qos, nullptr, StatusMask::none());

        if (nullptr == ReplyWriter_) {
            throw std::runtime_error("Reply writer initialization failed");
        }
    }

    bool is_stopped() { return stop_.load(); }

    void stop()
    {
        stop_.store(true);
        cv_.notify_all();
    }

    void reply_routine()
    {
        while (!is_stopped()) {
            std::unique_lock<std::mutex> lock(mtx_);

            cv_.wait(lock, [this]() -> bool { return requests_.size() > 0 || is_stopped(); });

            if (!is_stopped()) {
                Request request = requests_.front();
                requests_.pop();

                eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix =
                    eprosima::fastdds::rtps::iHandle2GUID(request.info.publication_handle)
                        .guidPrefix;
                LOG(info) << "ServerApp Processing request from client " << client_guid_prefix;

                if (!client_matched_status_.is_fully_unmatched(client_guid_prefix)) {
                    LOG(warning) << "ServerApp Ignoring request from already gone client "
                                 << client_guid_prefix;
                    continue;
                }

                if (!client_matched_status_.is_matched(client_guid_prefix)) {
                    LOG(warning) << "ServerApp Client " << client_guid_prefix
                                 << " not fully matched, saving request for later";
                    requests_.push(request);
                    continue;
                }

                T_ReplyType reply;
                if (request_callback_) {
                    request_callback_(*request.request, reply);
                }
                eprosima::fastdds::rtps::WriteParams write_params;
                eprosima::fastdds::rtps::SequenceNumber_t request_id =
                    request.info.sample_identity.sequence_number();
                write_params.related_sample_identity().writer_guid(
                    request.info.sample_identity.writer_guid());
                write_params.related_sample_identity().sequence_number(request_id);

                ReturnCode_t ret = RETCODE_OK;
                if (ret != ReplyWriter_->write(&reply, write_params)) {
                    LOG(error) << "ServerApp Failed to send reply to request with ID '"
                               << request_id << "' to client " << client_guid_prefix;
                    requests_.push(request);
                } else {
                    LOG(info) << "ServerApp Reply to request with ID '" << request_id
                              << "' sent to client " << client_guid_prefix << "ret: " << ret;
                }
            }
        }
    }

private:
    DomainParticipant *m_participant_;
    Subscriber *m_subscriber_;
    Publisher *m_publisher_;
    DataReader *RequestReader_;
    DataWriter *ReplyWriter_;
    TypeSupport RequestType_;
    TypeSupport ReplyType_;
    Topic *Replytopic_;
    Topic *RequestTopic_;
    std::mutex mtx_;
    std::atomic<bool> stop_;
    std::condition_variable cv_;
    RemoteClientMatchedStatus client_matched_status_;
    std::function<void(const T_RequestType &request, T_ReplyType &reply)> request_callback_;
    struct Request {
        SampleInfo info;
        std::shared_ptr<T_RequestType> request;
    };
    std::queue<Request> requests_;
    std::thread reply_thread_;

protected:
    void on_participant_discovery(DomainParticipant *participant,
                                  eprosima::fastdds::rtps::ParticipantDiscoveryStatus reason,
                                  const ParticipantBuiltinTopicData &info,
                                  bool &should_be_ignored) override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        should_be_ignored = false;
        eprosima::fastdds::rtps::GuidPrefix_t remote_participant_guid_prefix = info.guid.guidPrefix;
        std::string status_str = TypeConverter::to_string(reason);
        if (info.user_data.data_vec().size() != 1) {
            should_be_ignored = true;
            LOG(debug) << "ServerApp Ignoring participant with invalid user data: "
                       << remote_participant_guid_prefix;
        }

        if (!should_be_ignored) {
            if (reason
                == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT) {
                LOG(info) << "ServerApp"
                          << " " << status_str << ": " << remote_participant_guid_prefix;
            } else if (reason
                           == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::
                               REMOVED_PARTICIPANT
                       || reason
                              == eprosima::fastdds::rtps::ParticipantDiscoveryStatus::
                                  DROPPED_PARTICIPANT) {
                client_matched_status_.match_reply_reader(remote_participant_guid_prefix, false);
                client_matched_status_.match_request_writer(remote_participant_guid_prefix, false);

                LOG(info) << "ServerApp"
                          << " " << status_str << ": " << remote_participant_guid_prefix;
            }
        }
    }

    void on_publication_matched(DataWriter *writer, const PublicationMatchedStatus &info) override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix =
            eprosima::fastdds::rtps::iHandle2GUID(info.last_subscription_handle).guidPrefix;

        if (info.current_count_change == 1) {
            LOG(debug) << "ServerApp  Remote reply reader matched with client "
                       << client_guid_prefix;
            client_matched_status_.match_reply_reader(client_guid_prefix, true);
        } else if (info.current_count_change == -1) {
            LOG(debug) << "ServerApp  Remote reply reader unmatched from client "
                       << client_guid_prefix;
            client_matched_status_.match_reply_reader(client_guid_prefix, false);

            // Remove old replies since no one is waiting for them
            if (client_matched_status_.no_client_matched()) {
                std::size_t removed;
                ReplyWriter_->clear_history(&removed);
            }
        } else {
            LOG(error) << "ServerApp info.current_count_change is not a valid value for "
                          "PublicationMatchedStatus current "
                          "count change";
        }
    };

    void on_subscription_matched(DataReader *reader, const SubscriptionMatchedStatus &info) override
    {
        std::lock_guard<std::mutex> lock(mtx_);

        eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix =
            eprosima::fastdds::rtps::iHandle2GUID(info.last_publication_handle).guidPrefix;

        if (info.current_count_change == 1) {
            LOG(debug) << "ServerApp Remote request writer matched with client "
                       << client_guid_prefix;
            client_matched_status_.match_request_writer(client_guid_prefix, true);
        } else if (info.current_count_change == -1) {
            LOG(debug) << "ServerApp Remote request writer unmatched from client "
                       << client_guid_prefix;
            client_matched_status_.match_request_writer(client_guid_prefix, false);

            // Remove old replies since no one is waiting for them
            if (client_matched_status_.no_client_matched()) {
                std::size_t removed;
                ReplyWriter_->clear_history(&removed);
            }
        } else {
            LOG(error)
                << "ServerApp" << info.current_count_change
                << " is not a valid value for SubscriptionMatchedStatus current count change";
        }
    };

    void on_data_available(DataReader *reader) override
    {
        SampleInfo info;
        auto request = std::make_shared<T_RequestType>();

        while ((!is_stopped()) && (RETCODE_OK == reader->take_next_sample(request.get(), &info))) {
            if ((info.instance_state == ALIVE_INSTANCE_STATE) && info.valid_data) {
                eprosima::fastdds::rtps::GuidPrefix_t client_guid_prefix =
                    eprosima::fastdds::rtps::iHandle2GUID(info.publication_handle).guidPrefix;
                eprosima::fastdds::rtps::SequenceNumber_t request_id =
                    info.sample_identity.sequence_number();
                LOG(info) << "ServerApp Request with ID '" << request_id
                          << "' received from client " << client_guid_prefix;
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    requests_.push({info, request});
                    cv_.notify_all();
                }
            }
        }
    }
};
// namespace request_reply
} // namespace request_reply
#endif