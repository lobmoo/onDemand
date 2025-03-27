// Copyright 2024 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file PublisherApp.cpp
 *
 */

#include "PublisherApp.hpp"

#include <condition_variable>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/topic/TopicDescription.hpp>
#include <stdexcept>

#include "HelloWorldPubSubTypes.hpp"

using namespace eprosima::fastdds::dds;

namespace eprosima
{
namespace fastdds
{
    namespace examples
    {
        namespace hello_world
        {

            class DDSParticipantListener : public eprosima::fastdds::dds::DomainParticipantListener
            {
            public:
                // 当一个新的参与者被发现时调用
                virtual void
                on_participant_discovery(DomainParticipant *participant,
                                         eprosima::fastdds::rtps::ParticipantDiscoveryStatus reason,
                                         const ParticipantBuiltinTopicData &info,
                                         bool &should_be_ignored) override
                {
                    //should_be_ignored = false;
                    std::cout << "on_participant_discovery" << std::endl;
                }

                // 当数据读取者被发现时调用
                virtual void on_data_reader_discovery(
                    eprosima::fastdds::dds::DomainParticipant *participant,
                    eprosima::fastdds::rtps::ReaderDiscoveryStatus reason,
                    const eprosima::fastdds::dds::SubscriptionBuiltinTopicData &info,
                    bool &should_be_ignored) override
                {
                    //should_be_ignored = false;
                    std::cout << "on_data_reader_discovery" << std::endl;
                }

                // 另一个 on_data_writer_discovery 的重载版本
                virtual void on_data_writer_discovery(
                    eprosima::fastdds::dds::DomainParticipant *participant,
                    eprosima::fastdds::rtps::WriterDiscoveryStatus reason,
                    const eprosima::fastdds::dds::PublicationBuiltinTopicData &info,
                    bool &should_be_ignored) override
                {
                    //should_be_ignored = false;
                    std::cout << "on_data_writer_discovery" << std::endl;
                }
            };

            PublisherApp::PublisherApp(const CLIParser::publisher_config &config,
                                       const std::string &topic_name)
                : participant_(nullptr), publisher_(nullptr), topic_(nullptr), writer_(nullptr),
                  type_(new HelloWorldPubSubType()), matched_(0), samples_(config.samples),
                  expected_matches_(config.matched), stop_(false)
            {
                // Set up the data type with initial values
                hello_.index(0);
                hello_.message("Hello world");
                DDSParticipantListener *listener = new DDSParticipantListener();
                // Create the participant
                auto factory = DomainParticipantFactory::get_instance();
                participant_ =
                    factory->create_participant_with_default_profile(listener, StatusMask::all());
                if (participant_ == nullptr) {
                    throw std::runtime_error("Participant initialization failed");
                }

                // Register the type
                type_.register_type(participant_);

                // Create the publisher
                PublisherQos pub_qos = PUBLISHER_QOS_DEFAULT;
                participant_->get_default_publisher_qos(pub_qos);
                publisher_ = participant_->create_publisher(pub_qos, nullptr, StatusMask::none());
                if (publisher_ == nullptr) {
                    throw std::runtime_error("Publisher initialization failed");
                }

                // Create the topic
                TopicQos topic_qos = TOPIC_QOS_DEFAULT;
                participant_->get_default_topic_qos(topic_qos);
                topic_ = participant_->create_topic(topic_name, type_.get_type_name(), topic_qos);
                if (topic_ == nullptr) {
                    throw std::runtime_error("Topic initialization failed");
                }

                // Create the data writer
                DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;
                publisher_->get_default_datawriter_qos(writer_qos);
                writer_ =
                    publisher_->create_datawriter(topic_, writer_qos, this, StatusMask::all());
                if (writer_ == nullptr) {
                    throw std::runtime_error("DataWriter initialization failed");
                }
            }

            PublisherApp::~PublisherApp()
            {
                if (nullptr != participant_) {
                    // Delete DDS entities contained within the DomainParticipant
                    participant_->delete_contained_entities();

                    // Delete DomainParticipant
                    DomainParticipantFactory::get_instance()->delete_participant(participant_);
                }
            }

            void PublisherApp::on_publication_matched(DataWriter * /*writer*/,
                                                      const PublicationMatchedStatus &info)
            {
                if (info.current_count_change == 1) {
                    matched_ = static_cast<int16_t>(info.current_count);
                    std::cout << "Publisher matched." << std::endl;
                    cv_.notify_one();
                } else if (info.current_count_change == -1) {
                    matched_ = static_cast<int16_t>(info.current_count);
                    std::cout << "Publisher unmatched." << std::endl;
                } else {
                    std::cout
                        << info.current_count_change
                        << " is not a valid value for PublicationMatchedStatus current count change"
                        << std::endl;
                }
            }

            void PublisherApp::run()
            {
                while (!is_stopped() && ((samples_ == 0) || (hello_.index() < samples_))) {
                    if (publish()) {
                        std::cout << "Message: '" << hello_.message() << "' with index: '"
                                  << hello_.index() << "' SENT" << std::endl;
                    }
                    // Wait for period or stop event
                    std::unique_lock<std::mutex> period_lock(mutex_);
                    cv_.wait_for(period_lock, std::chrono::milliseconds(period_ms_),
                                 [&]() { return is_stopped(); });
                }
            }

            bool PublisherApp::publish()
            {
                bool ret = false;
                // Wait for the data endpoints discovery
                std::unique_lock<std::mutex> matched_lock(mutex_);
                cv_.wait(matched_lock, [&]() {
                    // at least one has been discovered
                    return ((matched_ >= expected_matches_) || is_stopped());
                });

                if (!is_stopped()) {
                    hello_.index(hello_.index() + 1);
                    ret = (RETCODE_OK == writer_->write(&hello_));
                }
                return ret;
            }

            bool PublisherApp::is_stopped() { return stop_.load(); }

            void PublisherApp::stop()
            {
                stop_.store(true);
                cv_.notify_one();
            }

        } // namespace hello_world
    }     // namespace examples
} // namespace fastdds
} // namespace eprosima
