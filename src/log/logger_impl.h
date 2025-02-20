#ifndef LOGGER_IMPL_H
#define LOGGER_IMPL_H

#include <spdlog/async.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <vector>

#include "custom_rotating_file_sink.h"
#include "logger.h"
class Logger::LoggerImpl {
 public:
  LoggerImpl();
  ~LoggerImpl();

  bool Init(std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync);
  void log(Logger::severity_level level, const std::string& msg, const char* file, int line, const char* func);
  void Uinit();
  void setFlushEvery(uint32_t flushEvery);
  void setFlushOnLevel(Logger::severity_level flushOnLevel);

 private:
  std::shared_ptr<spdlog::logger> logger = nullptr;
  uint32_t flushEvery_;
  spdlog::level::level_enum flushOnLevel_;

 private:
  spdlog::level::level_enum GetLogLevelFromEnv(const std::string& env_var, spdlog::level::level_enum default_level);

};

#endif  // LOGGER_IMPL_H