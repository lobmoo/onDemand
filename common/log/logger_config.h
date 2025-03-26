/**
 * @file logger_config.h
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

#ifndef LOGGER_CONFIG_H
#define LOGGER_CONFIG_H

#include <string>

class LoggerConfig {
 private:
  LoggerConfig(/* args */);
  struct LoggerConfigData {
    std::string log_file_name;
    int log_type;
    int log_level;
    int max_file_size;
    int max_backup_index;
    bool is_async;
  };

 public:
  LoggerConfig *Instance() {
    static LoggerConfig config;
    return &config;
  }

  ~LoggerConfig();

  public:
  
};

LoggerConfig::~LoggerConfig() {}

#endif  // LOGGER_CONFIG_H