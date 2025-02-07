#include "logger_impl.h"

#include <iomanip>
#include <memory>
#include <sstream>

Logger::LoggerImpl::LoggerImpl() {}
Logger::LoggerImpl::~LoggerImpl() {}

bool Logger::LoggerImpl::Init(
    std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  constexpr std::size_t log_buffer_size = 32 * 1024;
  std::size_t max_file_size = 1024 * 1024 * maxFileSize;
  std::string logName = getLogNameInfo(fileName);
  std::vector<spdlog::sink_ptr> sinks;

  spdlog::level::level_enum console_level = GetLogLevelFromEnv("LOG_CONSOLE_LEVEL", spdlog::level::info);
  spdlog::level::level_enum file_level = GetLogLevelFromEnv("LOG_FILE_LEVEL", spdlog::level::info);

  switch (type) {
    case Logger::both: {
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(fileName, max_file_size, maxBackupIndex);
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
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(fileName, max_file_size, maxBackupIndex);
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
  logger->set_level(static_cast<spdlog::level::level_enum>(level));
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%# %!] %v");
  logger->flush_on(spdlog::level::err);
  spdlog::register_logger(logger);
  spdlog::set_default_logger(logger);
  return true;
}

void Logger::LoggerImpl::log(
    Logger::severity_level level, const std::string& msg, const char* file, int line, const char* func) {
  if (logger) {
    logger->log(spdlog::source_loc{file, line, func}, static_cast<spdlog::level::level_enum>(level), msg);
  }
}

spdlog::level::level_enum Logger::LoggerImpl::GetLogLevelFromEnv(const std::string& env_var, spdlog::level::level_enum default_level) {
  const char* env_value = std::getenv(env_var.c_str());
  if (!env_value) {
    return default_level;
  }
  std::string level_str(env_value);
  return spdlog::level::from_str(level_str);
}

std::string Logger::LoggerImpl::getLogNameInfo(const std::string& fileName) {
  pid_t pid = getpid();

  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm = *std::localtime(&time);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");

  char process_name[256];
  if (readlink("/proc/self/exe", process_name, sizeof(process_name) - 1) != -1) {
    process_name[sizeof(process_name) - 1] = '\0';
  } else {
    std::strncpy(process_name, "unknown", sizeof(process_name));
  }

  std::ostringstream file_name;
  file_name << fileName << "_" << process_name << "_" << oss.str() << "_pid" << pid << ".log";
  return file_name.str();
}


