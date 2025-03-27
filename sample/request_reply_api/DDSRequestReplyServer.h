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

class DDSRequestReplyServer : public DomainParticipantListener
{
public:
    DDSRequestReplyServer(const std::string &service_name = SERVER_NAME);
    ~DDSRequestReplyServer();

private:
    bool create_participant();
    void create_request_entities(const std::string &service_name);
    void create_reply_entities(const std::string &service_name);
    bool is_stopped();
    void reply_routine();
    bool calculate(const CalculatorRequestType &request, std::int32_t &result);

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

    struct Request {
        SampleInfo info;
        std::shared_ptr<CalculatorRequestType> request;
    };
    std::queue<Request> requests_;
    std::thread reply_thread_;

protected:
    void on_participant_discovery(DomainParticipant *participant,
                                  eprosima::fastdds::rtps::ParticipantDiscoveryStatus status,
                                  const ParticipantBuiltinTopicData &info,
                                  bool &should_be_ignored) override;

    void on_publication_matched(DataWriter *writer, const PublicationMatchedStatus &info) override;

    void on_subscription_matched(DataReader *reader,
                                 const SubscriptionMatchedStatus &info) override;

    void on_data_available(DataReader *reader) override;
};
// namespace request_reply
} // namespace request_reply
#endif