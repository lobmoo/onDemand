/**
 * @file MonitorApi.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-06-05
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-06-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */


#pragma once
#include <memory>
#include <string>
#include <vector>

namespace Monitor {


enum R_W_Type
{
    R_TYPE = 0,
    W_TYPE = 1,
};

struct info_t{
    R_W_Type type;
    std::string name;
};


struct topicInfo_t {  // 要填充满
    std::string data_type;
    std::string domainId;
    std::vector<std::string> datareaders;
    std::vector<std::string> datawriters;
    std::vector<info_t> participants; 
    std::vector<info_t> users; 
    std::vector<info_t> processes; 
    std::vector<info_t> hosts; //TODO 可以扩展所有信息 
};


class MonitorAPI {
public:
    MonitorAPI();
    ~MonitorAPI();
    bool init(uint32_t domain, uint16_t n_bins, uint32_t t_interval);
    bool stop();
    void run();
    topicInfo_t getTopicInfo(const std::string& topicName) const;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}