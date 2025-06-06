/**
 * @file Monitor.h
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
#ifndef MONITOR_H
#define MONITOR_H

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <string>

#include <fastdds_statistics_backend/exception/Exception.hpp>
#include <fastdds_statistics_backend/listener/DomainListener.hpp>
#include <fastdds_statistics_backend/StatisticsBackend.hpp>
#include <fastdds_statistics_backend/types/EntityId.hpp>
#include <fastdds_statistics_backend/types/types.hpp>
#include <fastdds_statistics_backend/types/JSONTags.h>

using namespace eprosima::statistics_backend;
namespace Monitor
{
class Monitor
{
public:
    Monitor();

    virtual ~Monitor();

    //! Initialize monitor
    bool init(uint32_t domain, uint16_t n_bins, uint32_t t_interval, std::string dump_file = "",
              bool reset = false);

    void run();


    static bool is_stopped();


    static void stop();


    std::vector<StatisticsData> get_fastdds_latency_mean();

 
    std::vector<StatisticsData> get_publication_throughput_mean();

   
    std::string timestamp_to_string(const Timestamp timestamp);

protected:
 
    void dump_in_file();


    void clear_inactive_entities();

    class Listener : public eprosima::statistics_backend::PhysicalListener
    {
    public:
        Listener() {}

        ~Listener() override {}

        
        void on_host_discovery(EntityId host_id, const DomainListener::Status &status) override;

       
        void on_user_discovery(EntityId user_id, const DomainListener::Status &status) override;

       
        void on_process_discovery(EntityId process_id,
                                  const DomainListener::Status &status) override;

    
        void on_topic_discovery(EntityId domain_id, EntityId topic_id,
                                const DomainListener::Status &status) override;

        void on_participant_discovery(EntityId domain_id, EntityId participant_id,
                                      const DomainListener::Status &status) override;

        
        void on_datareader_discovery(EntityId domain_id, EntityId datareader_id,
                                     const DomainListener::Status &status) override;

       
        void on_datawriter_discovery(EntityId domain_id, EntityId datawriter_id,
                                     const DomainListener::Status &status) override;

    } physical_listener_;

    EntityId monitor_id_;

private:
  
    std::vector<std::string> user_data_;

    
    uint16_t n_bins_;

   
    uint32_t t_interval_;

   
    std::string dump_file_;

   
    bool reset_;

  
    static std::atomic<bool> stop_;

   
    static std::mutex terminate_cv_mtx_;

    
    static std::condition_variable terminate_cv_;
};
} // namespace Monitor
#endif
