#ifndef LOGGER_IMPL_H
#define LOGGER_IMPL_H

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <vector>

#include "logger.h"

class Logger::LoggerImpl {
 public:
  LoggerImpl();
  ~LoggerImpl();

  bool Init(std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync);
  void setConsoleLogLevel(Logger::severity_level level);
  void setFileLogLevel(Logger::severity_level level);
  void log(Logger::severity_level level, const std::string& msg, const char* file, int line);

 private:
  std::vector<spdlog::sink_ptr> sinks;
  std::shared_ptr<spdlog::logger> logger = nullptr;
  std::string getLogNameInfo();

 public:
};

#endif  // LOGGER_IMPL_H