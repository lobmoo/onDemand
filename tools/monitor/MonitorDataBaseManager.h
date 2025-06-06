/**
 * @file MonitorDataBaseManager.h
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
#ifndef MONITORDATABASEMANAGER_H_
#define MONITORDATABASEMANAGER_H_
#include "MonitorDataBase.h"
#include "MonitorApi.h"

#include "Monitor.h"

namespace Monitor
{
  
// enum R_W_Type
// {
//     R_TYPE = 0,
//     W_TYPE = 1,
// };

// struct info_t{
//     R_W_Type type;
//     std::string name;
// };

// struct topicInfo_t {  // 要填充满
//     std::string data_type;
//     std::string domainId;
//     std::vector<std::string> datareaders;
//     std::vector<std::string> datawriters;
//     std::vector<info_t> participants; 
//     std::vector<info_t> users; 
//     std::vector<info_t> processes; 
//     std::vector<info_t> hosts; //TODO 可以扩展所有信息 
// };

class MonitorDataBaseManager
{

public:
    static MonitorDataBaseManager &getInstance()
    {
        static MonitorDataBaseManager instance;
        return instance;
    }

    bool init(uint32_t domain, uint16_t n_bins, uint32_t t_interval, const std::string &dump_file = "", bool reset = false);
    bool stop();
    void run();
    bool parseTopic(const std::string &topicName);
    topicInfo_t getTopicInfo(const std::string &topicName) const;
    void printTopicInfo(const std::string &topicName);
    std::string getProcessNameByPid(const std::string& pid_str); // 根据进程号去查询进程名

private:
    MonitorDataBaseManager();
    ~MonitorDataBaseManager();
    MonitorDataBaseManager(const MonitorDataBaseManager &) = delete;
    MonitorDataBaseManager &operator=(const MonitorDataBaseManager &) = delete;
    topicInfo_t getTopicInfoInternal(const std::string &topicName) const;

private:
    std::unordered_map<std::string, topicInfo_t> topicInfo_;
    std::mutex mutex_;
    std::unique_ptr<Monitor> monitor_;
    uint32_t t_interval_;
    std::atomic<bool> running_;
};

} // namespace Monitor

#endif // MonitorDataBaseManager