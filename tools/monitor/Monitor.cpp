/**
 * @file Monitor.cpp
 * 
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-05-21
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-05-21     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#include <chrono>
#include <csignal>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fastdds_statistics_backend/listener/DomainListener.hpp>
#include <fastdds_statistics_backend/StatisticsBackend.hpp>
#include <fastdds_statistics_backend/types/EntityId.hpp>
#include <fastdds_statistics_backend/types/types.hpp>
#include <fastdds_statistics_backend/types/utils.hpp>

#include "log/logger.h"
#include "Monitor.h"
#include "MonitorDataBase.h"
#include "MonitorDataBaseManager.h"
namespace Monitor
{
using namespace eprosima::statistics_backend;

std::atomic<bool> Monitor::stop_(false);
std::mutex Monitor::terminate_cv_mtx_;
std::condition_variable Monitor::terminate_cv_;

Monitor::Monitor()
{
}

Monitor::~Monitor()
{
    StatisticsBackend::stop_monitor(monitor_id_);
}

bool Monitor::is_stopped()
{
    return stop_;
}

void Monitor::stop()
{
    stop_ = true;
    terminate_cv_.notify_all();
}

bool Monitor::init(uint32_t domain, uint16_t n_bins, uint32_t t_interval,
                   std::string dump_file /* = "" */, bool reset /* = false */)
{
    n_bins_ = n_bins;
    t_interval_ = t_interval;
    monitor_id_ = StatisticsBackend::init_monitor(domain);
    dump_file_ = dump_file;
    reset_ = reset;

    if (!monitor_id_.is_valid()) {
        LOG(info) << "Error creating monitor";
        return 1;
    }

    StatisticsBackend::set_physical_listener(&physical_listener_);

    return true;
}

void Monitor::run()
{
    //stop_ = false;
    // LOG(info) << "Monitor running. Please press CTRL+C to stop the Monitor at any time.";
    // signal(SIGINT, [](int signum) {
    //     LOG(info) << "SIGINT received, stopping Monitor execution.";
    //     static_cast<void>(signum);
    //     Monitor::stop();
    // });

    while (!is_stopped()) {
        std::unique_lock<std::mutex> lck(terminate_cv_mtx_);
        terminate_cv_.wait_for(lck, std::chrono::milliseconds(t_interval_), [] { return is_stopped(); });
        get_fastdds_latency_mean();
        get_publication_throughput_mean();

        if (!dump_file_.empty()) {
            dump_in_file();
        }
        // 这里清除inactive entity和statistics数据试试
        StatisticsBackend::clear_inactive_entities();
        // StatisticsBackend::clear_statistics_data();
        // 将database类内数据清空
        MonitorDataBase::getInstance().reset();
        /*json 解析管理数据*/

        
        auto dump = StatisticsBackend::dump_database(reset_);
        json &j = dump;
        try {
            MonitorDataBase::getInstance().parse(j);
            //MonitorDataBase::getInstance().print();
        } catch (const std::exception &e) {
            std::cerr << "Failed to parse datareader: " << e.what() << std::endl;
        }

        if (reset_) {
            clear_inactive_entities();
            LOG(info) << "Removing inactive entities from Statistics Backend.";
        }
    }
    return;
}

void Monitor::dump_in_file()
{
    auto t = std::time(nullptr);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    std::string current_time = oss.str();

    std::string complete_file_name = dump_file_ + "_" + current_time + ".json";
    LOG(info) << "Dumping info in file " << complete_file_name;

    auto dump = StatisticsBackend::dump_database(reset_);

    std::ofstream file(complete_file_name);
    file << std::setw(4) << dump;
}

void Monitor::clear_inactive_entities()
{
    StatisticsBackend::clear_inactive_entities();
}

std::vector<StatisticsData> Monitor::get_fastdds_latency_mean()
{
    std::vector<StatisticsData> latency_data{};

    std::vector<EntityId> topics = StatisticsBackend::get_entities(EntityKind::TOPIC);
    EntityId m_topic_id = -1;
    Info topic_info;
    for (auto topic_id : topics) {
        topic_info = StatisticsBackend::get_info(topic_id);
        if (topic_info[NAME_TAG] == "wwk" && topic_info[DATA_TYPE_TAG] == "HelloWorldOne") {
            m_topic_id = topic_id;
        }

        LOG(debug) << "Topic with ID " << topic_id
                  << " discovered. Name: " << std::string(topic_info[NAME_TAG])
                  << ", Data type: " << std::string(topic_info[DATA_TYPE_TAG]);
    }

    if (m_topic_id < 0) {
        return latency_data;
    }

    auto statistics_topic_info = StatisticsBackend::get_info(m_topic_id);
    /* Get the DataWriters and DataReaders in a Topic */
    std::vector<EntityId> topic_datawriters =
        StatisticsBackend::get_entities(EntityKind::DATAWRITER, m_topic_id);
    std::vector<EntityId> topic_datareaders =
        StatisticsBackend::get_entities(EntityKind::DATAREADER, m_topic_id);

    /* Get the current time */
    auto now_time = now();

    latency_data =
        StatisticsBackend::get_data(DataKind::FASTDDS_LATENCY,                    // DataKind
                                    topic_datawriters,                            // Source entities
                                    topic_datareaders,                            // Target entities
                                    n_bins_,                                      // Number of bins
                                    now_time - std::chrono::seconds(t_interval_), // t_from
                                    now_time,                                     // t_to
                                    StatisticKind::MEAN);                         // Statistic

    for (auto latency : latency_data) {

        LOG(debug) << "Fast DDS Latency of HelloWorld topic:  [" << statistics_topic_info[NAME_TAG]
                  << "]: [" << timestamp_to_string(latency.first) << ", " << latency.second / 1000
                  << " μs]";
    }

    return latency_data;
}

std::vector<StatisticsData> Monitor::get_publication_throughput_mean()
{
    std::vector<StatisticsData> publication_throughput_data{};

    std::vector<EntityId> participants = StatisticsBackend::get_entities(EntityKind::PARTICIPANT);
    EntityId participant_id = -1;
    Info participant_info;
    for (auto participant : participants) {
        participant_info = StatisticsBackend::get_info(participant);
        if (participant_info[NAME_TAG] == "Participant_pub" && participant_info[ALIVE_TAG]) {
            participant_id = participant;
            break;
        }
    }

    if (participant_id < 0) {
        return publication_throughput_data;
    }

    /* Get the DataWriters and DataReaders in a Topic */
    std::vector<EntityId> topic_datawriters =
        StatisticsBackend::get_entities(EntityKind::DATAWRITER, participant_id);

    /* Get the current time */
    auto now_time = now();

    /*
     *
     */
    publication_throughput_data =
        StatisticsBackend::get_data(DataKind::PUBLICATION_THROUGHPUT,             // DataKind
                                    topic_datawriters,                            // Source entities
                                    n_bins_,                                      // Number of bins
                                    now_time - std::chrono::seconds(t_interval_), // t_from
                                    now_time,                                     // t_to
                                    StatisticKind::MEAN);                         // Statistic

    for (auto publication_throughput : publication_throughput_data) {

        LOG(debug) << "Publication throughput of Participant "
                  << std::string(participant_info[NAME_TAG]) << ": ["
                  << timestamp_to_string(publication_throughput.first) << ", "
                  << publication_throughput.second << " B/s]";
    }

    return publication_throughput_data;
}

void Monitor::Listener::on_participant_discovery(EntityId domain_id, EntityId participant_id,
                                                 const DomainListener::Status &status)
{
    static_cast<void>(domain_id);
    Info participant_info = StatisticsBackend::get_info(participant_id);

    if (status.current_count_change == 1) {
        LOG(debug) << "Participant with GUID " << std::string(participant_info[GUID_TAG])
                  << " discovered.";
    } else {
        LOG(debug) << "Participant with GUID " << std::string(participant_info[GUID_TAG])
                  << " update info.";
    }
}

void Monitor::Listener::on_datareader_discovery(EntityId domain_id, EntityId datareader_id,
                                                const DomainListener::Status &status)
{
    static_cast<void>(domain_id);
    Info datareader_info = StatisticsBackend::get_info(datareader_id);

    if (!datareader_info[METATRAFFIC_TAG]) {
        if (status.current_count_change == 1) {
            LOG(debug) << "DataReader with GUID " << std::string(datareader_info[GUID_TAG])
                      << " discovered.";
        } else {
            LOG(debug) << "DataReader with GUID " << std::string(datareader_info[GUID_TAG])
                      << " update info.";
        }
    }
}

void Monitor::Listener::on_datawriter_discovery(EntityId domain_id, EntityId datawriter_id,
                                                const DomainListener::Status &status)
{
    static_cast<void>(domain_id);
    Info datawriter_info = StatisticsBackend::get_info(datawriter_id);

    if (!datawriter_info[METATRAFFIC_TAG]) {
        if (status.current_count_change == 1) {
            LOG(debug) << "DataWriter with GUID " << std::string(datawriter_info[GUID_TAG])
                      << " discovered.";
        } else {
            LOG(debug) << "DataWriter with GUID " << std::string(datawriter_info[GUID_TAG])
                      << " update info.";
        }
    }
}

void Monitor::Listener::on_host_discovery(EntityId host_id, const DomainListener::Status &status)
{
    Info host_info = StatisticsBackend::get_info(host_id);

    if (status.current_count_change == 1) {
        LOG(debug) << "Host " << std::string(host_info[NAME_TAG]) << " discovered.";
    } else {
        LOG(debug) << "Host " << std::string(host_info[NAME_TAG]) << " update info.";
    }
}

void Monitor::Listener::on_user_discovery(EntityId user_id, const DomainListener::Status &status)
{
    Info user_info = StatisticsBackend::get_info(user_id);

    //user_data_.
    if (status.current_count_change == 1) {
        LOG(debug) << "User " << std::string(user_info[NAME_TAG]) << " discovered.";
    } else {
        LOG(debug) << "User " << std::string(user_info[NAME_TAG]) << " update info.";
    }
}

void Monitor::Listener::on_process_discovery(EntityId process_id,
                                             const DomainListener::Status &status)
{
    Info process_info = StatisticsBackend::get_info(process_id);

    if (status.current_count_change == 1) {
        LOG(debug) << "Process " << std::string(process_info[NAME_TAG]) << " discovered.";
    } else {
        LOG(debug) << "Process " << std::string(process_info[NAME_TAG]) << " update info.";
    }
}

void Monitor::Listener::on_topic_discovery(EntityId domain_id, EntityId topic_id,
                                           const DomainListener::Status &status)
{
    static_cast<void>(domain_id);
    Info topic_info = StatisticsBackend::get_info(topic_id);

    if (!topic_info[METATRAFFIC_TAG]) {
        if (status.current_count_change == 1) {
            LOG(debug) << "Topic " << std::string(topic_info[NAME_TAG]) << " discovered.";
        } else {
            LOG(debug) << "Topic " << std::string(topic_info[NAME_TAG]) << " update info.";
        }
    }
}

std::string Monitor::timestamp_to_string(const Timestamp timestamp)
{
    auto timestamp_t = std::chrono::system_clock::to_time_t(timestamp);
    auto msec =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
    msec %= 1000;
    std::stringstream ss;

#ifdef _WIN32
    struct tm timestamp_tm;
    _localtime64_s(&timestamp_tm, &timestamp_t);
    ss << std::put_time(&timestamp_tm, "%F %T") << "." << std::setw(3) << std::setfill('0') << msec;
#else
    ss << std::put_time(localtime(&timestamp_t), "%F %T") << "." << std::setw(3)
       << std::setfill('0') << msec;
#endif // ifdef _WIN32

    return ss.str();
}
} // namespace Monitor
