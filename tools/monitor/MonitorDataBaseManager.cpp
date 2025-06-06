/**
 * @file MonitorDataBaseManager.cc
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
#include <thread>
#include <fstream>
#include "MonitorDataBaseManager.h"
#include "log/logger.h"

namespace Monitor
{

MonitorDataBaseManager::MonitorDataBaseManager():running_(true)
{

}

MonitorDataBaseManager::~MonitorDataBaseManager()
{
    
}

bool MonitorDataBaseManager::init(uint32_t domain, uint16_t n_bins, uint32_t t_interval, const std::string &dump_file, bool reset)
{
    try {
        monitor_ = std::make_unique<Monitor>();
        if (!monitor_) {
            LOG(error) << "MonitorDataBaseManager init failed: monitor is nullptr";
            return false;
        }
        if (monitor_->init(static_cast<uint32_t>(domain), static_cast<uint16_t>(n_bins),
                           static_cast<uint32_t>(t_interval), dump_file, reset)) {
            std::thread([this]() { monitor_->run(); }).detach();
        }
        t_interval_ = t_interval;
    } catch (const std::exception &e) {
        LOG(error) << "MonitorDataBaseManager init failed: " << e.what();
        return false;
    }
    return true;
}

bool MonitorDataBaseManager::stop()
{
    running_ = false;
    if (monitor_) {
        monitor_->stop();
        monitor_.reset();
    } else {
        LOG(error) << "MonitorDataBaseManager stop failed: monitor is nullptr";
        return false;
    }
    LOG(info) << "MonitorDataBaseManager stopped successfully";
    return true;
}

bool MonitorDataBaseManager::parseTopic(const std::string &topicName)
{
    
    if (topicName.empty()) {
        LOG(error) << "Topic name is empty";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto &DataBase = MonitorDataBase::getInstance();
    try {
        topicInfo_t topicInfo{};

        auto parseParticipant = [&DataBase, &topicInfo](const std::string &Id, 
                                                        R_W_Type type) {
            /*处理逻辑都写到这里*/
            const Participant &participant = DataBase.getParticipant(EntityID("participants", Id));
            info_t participantInfo;
            participantInfo.type = type; 
            participantInfo.name = participant.name;
            topicInfo.participants.push_back(participantInfo);
            /*TODO*/
            /*查询 process */
            const Process &process = DataBase.getProcess(EntityID("processes", participant.process));
            info_t processInfo;
            processInfo.type = type;
            processInfo.name = process.name;
            topicInfo.processes.push_back(processInfo);

            /*查询 user */
            const User &user = DataBase.getUser(EntityID("users", process.user));
            info_t userInfo;
            userInfo.type = type;
            userInfo.name = user.name;
            topicInfo.users.push_back(userInfo);

            /*查询 host */
            const Host &host = DataBase.getHost(EntityID("hosts", user.host));
            info_t hostInfo;
            hostInfo.type = type;
            hostInfo.name = host.name;
            topicInfo.hosts.push_back(hostInfo);
        };

        /*拿到id*/
        EntityID topicId = DataBase.getTopicID(topicName);
        /*拿到topic信息*/
        const Topic &topic = DataBase.getTopic(topicId);
        topicInfo.data_type = topic.data_type;
        topicInfo.domainId = topic.domain;

        /*查询 reader */
        for (auto &readerId : topic.datareaders) {
            const DataReader &datareader =
                DataBase.getDataReader(EntityID("datareaders", readerId));
            topicInfo.datareaders.push_back(datareader.name);
            /*获取participant的id*/
            std::string participantId = datareader.participant;
            parseParticipant(participantId, R_TYPE);
        }

        /*查询 writer */
        for (auto &writerId : topic.datawriters) {
            const DataWriter &datawriter =
                DataBase.getDataWriter(EntityID("datawriters", writerId));
            topicInfo.datawriters.push_back(datawriter.name);
            /*获取participant的id*/
            std::string participantId = datawriter.participant;
            parseParticipant(participantId, W_TYPE);
        }

    
        topicInfo_[topicName] = topicInfo;

    } catch (const std::out_of_range &e) {
        LOG(error) << "parseTopic failed, " << e.what();
        return false;
    }

    return true;
}

void MonitorDataBaseManager::run()
{
    while(running_)
    {
        // 遍历所有topic，然后都解析一遍
        auto &DataBase = MonitorDataBase::getInstance();
        const std::vector<std::string> &allTopicsName = DataBase.getAllTopicsName();

        for (const auto &topicName : allTopicsName) {
            if(!parseTopic(topicName)) {
                LOG(error) << "Failed to parse topic: " << topicName;
            } else {
                // 输出topic列表
                LOG(info) << "Monitoring topics: ";
                for (const auto &topicName : allTopicsName) {
                    std::cout << topicName << "  ";
                }
                std::cout << "\n";
                LOG(info) << "Parsed topic: " << topicName;
                printTopicInfo(topicName);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(t_interval_));
    }
}

void MonitorDataBaseManager::printTopicInfo(const std::string &topicName)
{
    // 输出topic信息
    auto topicInfo = getTopicInfo(topicName);

    std::cout << "Topic Name: " << topicName << "\n";
    std::cout << "Data Type: " << topicInfo.data_type << "\n";
    std::cout << "Domain ID: " << topicInfo.domainId << "\n";
    std::cout << "Data Readers: " << "\n";
    for (const auto &reader : topicInfo.datareaders) {
        std::cout << "  - " << reader << " (Reader)" << "\n";
    }
    std::cout << "Data Writers: " << "\n";
    for (const auto &writer : topicInfo.datawriters) {
        std::cout << "  - " << writer << " (Writer)" << "\n";
    }
    std::cout << "Participants: " << "\n";
    for (const auto &participant : topicInfo.participants) {
        std::cout << "  - " << participant.name << " (Type: "
                  << (participant.type == R_TYPE ? "Reader" : "Writer") << ")" << "\n";
    }
    std::cout << "Processes: " << "\n";
    for (const auto &process : topicInfo.processes) {
        const std::string procName = getProcessNameByPid(process.name);
        std::cout << "  - " << procName << " (Type: "
                  << (process.type == R_TYPE ? "Reader" : "Writer") << ")" << "\n";
    }
    
    std::cout << "Users: " << "\n";
    for (const auto &user : topicInfo.users) {
        std::cout << "  - " << user.name << " (Type: "
                  << (user.type == R_TYPE ? "Reader" : "Writer") << ")" << "\n";
    }

    std::cout << "Hosts: " << "\n";
    for (const auto &host : topicInfo.hosts) {
        std::cout << "  - " << host.name << " (Type: "
                  << (host.type == R_TYPE ? "Reader" : "Writer") << ")" << "\n";
    }

    return ;
}
topicInfo_t MonitorDataBaseManager::getTopicInfo(const std::string &topicName) const
{
    auto it = topicInfo_.find(topicName);
    if (it != topicInfo_.end()) {
        return it->second;
    } else {
        LOG(error) << "Topic " << topicName << " not found";
        return {};
    }
}

std::string MonitorDataBaseManager::getProcessNameByPid(const std::string& pid_str) {
    // 检查是否为合法数字
    if (pid_str.empty() || !std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) {
        std::cerr << "Invalid PID string: not a numeric value" << std::endl;
        return "";
    }

    // 转换为 pid_t 类型（int）
    pid_t pid = static_cast<pid_t>(std::stoi(pid_str));

    // 构建路径
    std::ostringstream path;
    path << "/proc/" << pid << "/comm";

    std::ifstream file(path.str());
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << path.str() << std::endl;
        return "";
    }

    std::string name;
    std::getline(file, name);
    return name;
}

} // namespace Monitor