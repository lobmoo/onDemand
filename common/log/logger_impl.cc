#include "logger_impl.h"

#include <iomanip>
#include <memory>
#include <sstream>

Logger::LoggerImpl::LoggerImpl():flushEvery_(0), flushOnLevel_(spdlog::level::err) {}
Logger::LoggerImpl::~LoggerImpl() {}

bool Logger::LoggerImpl::Init(
    std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  constexpr std::size_t log_buffer_size = 32 * 1024;
  std::size_t max_file_size = 1024 * 1024 * maxFileSize;
  std::vector<spdlog::sink_ptr> sinks;

  spdlog::level::level_enum console_level = GetLogLevelFromEnv("LOG_CONSOLE_LEVEL", spdlog::level::info);
  spdlog::level::level_enum file_level = GetLogLevelFromEnv("LOG_FILE_LEVEL", spdlog::level::info);

  switch (type) {
    case Logger::both: {
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      auto file_sink =
          std::make_shared<spdlog::sinks::custom_rotating_file_sink_mt>(fileName, max_file_size, maxBackupIndex);
      /*按天数轮转记录器  todo*/
      // auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(fileName, 23, 59);
      console_sink->set_level(console_level);
      file_sink->set_level(file_level);
      sinks.push_back(file_sink);
      sinks.push_back(console_sink);
    } break;
    case Logger::console: {
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(console_level);
      sinks.push_back(console_sink);
    } break;
    case Logger::file: {
      auto file_sink =
          std::make_shared<spdlog::sinks::custom_rotating_file_sink_mt>(fileName, max_file_size, maxBackupIndex);
      file_sink->set_level(file_level);
      sinks.push_back(file_sink);
    } break;
    default:
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(console_level);
      sinks.push_back(console_sink);
      break;
  }
  if (sinks.empty()) {
    return false;
  }
  if (isAsync) {
    spdlog::init_thread_pool(log_buffer_size, std::thread::hardware_concurrency() / 4);
    logger = std::make_shared<spdlog::async_logger>(
        "Logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
  } else {
    logger = std::make_shared<spdlog::logger>("Logger", sinks.begin(), sinks.end());
  }
  logger->set_level(static_cast<spdlog::level::level_enum>(spdlog::level::trace));
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%# %!] %v");
  logger->flush_on(flushOnLevel_);
  
  if(flushEvery_ > 0)
  {
    spdlog::flush_every(std::chrono::seconds(flushEvery_));
  }
  
  spdlog::register_logger(logger);
  spdlog::set_default_logger(logger);
  return true;
}

void Logger::LoggerImpl::Uinit() {
  if (logger) {
    logger->flush();
    spdlog::drop("Logger");
    logger.reset();
  }
  spdlog::shutdown();
}

void Logger::LoggerImpl::log(
    Logger::severity_level level, const std::string& msg, const char* file, int line, const char* func) {
  if (logger) {
    logger->log(spdlog::source_loc{file, line, func}, static_cast<spdlog::level::level_enum>(level), msg);
  }
}

spdlog::level::level_enum Logger::LoggerImpl::GetLogLevelFromEnv(
    const std::string& env_var, spdlog::level::level_enum default_level) {
  const char* env_value = std::getenv(env_var.c_str());
  if (!env_value) {
    return default_level;
  }
  std::string level_str(env_value);
  return spdlog::level::from_str(level_str);
}

void Logger::LoggerImpl::setFlushEvery(uint32_t flushEvery)
{
  if(flushEvery > 0)
  {
    flushEvery_ = flushEvery;
  }
}

void Logger::LoggerImpl::setFlushOnLevel(Logger::severity_level flushOnLevel)
{
  flushOnLevel_ = static_cast<spdlog::level::level_enum>(flushOnLevel);
}