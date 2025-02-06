#include "logger_impl.h"

#include <filesystem>
#include <memory>
#include <sstream>

Logger::LoggerImpl::LoggerImpl() {}
Logger::LoggerImpl::~LoggerImpl() {}

bool Logger::LoggerImpl::Init(
    std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  // namespace fs = std::filesystem;
  // fs::path log_path(fileName);
  // fs::path log_dir = log_path.parent_path();
  // if (!fs::exists(log_path)) {
  //   fs::create_directories(log_dir);
  // }
  constexpr std::size_t log_buffer_size = 32 * 1024;  // 32kb
  std::size_t max_file_size = 1024 * 1024 * maxFileSize;
  spdlog::init_thread_pool(log_buffer_size, std::thread::hardware_concurrency());

  switch (type) {
    case Logger::both: {
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      auto file_sink =
          std::make_shared<spdlog::sinks::rotating_file_sink_mt>(fileName, max_file_size, maxBackupIndex);
      sinks.push_back(file_sink);
      sinks.push_back(console_sink);
    } break;
    case Logger::console: {
    } break;
    case Logger::file: {
    } break;
    default:
      break;
  }
  if (isAsync) {
  } else {
    if (sinks.empty()) {
      return false;
    }
    logger = std::make_shared<spdlog::logger>("Logger", sinks.begin(), sinks.end());
    logger->set_level(static_cast<spdlog::level::level_enum>(level));
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
  }
  return true;
}

void Logger::LoggerImpl::log(Logger::severity_level level, const std::string& msg, const char* file, int line) {
  if (logger) {
    logger->log(spdlog::source_loc{file, line, SPDLOG_FUNCTION}, static_cast<spdlog::level::level_enum>(level), msg);
  }
}

void Logger::LoggerImpl::setConsoleLogLevel(Logger::severity_level level) {
  // if (consoleSink_) {
  //   switch (level) {
  //     case Logger::trace:
  //       consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::trace);
  //       break;
  //     case Logger::debug:
  //       consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::debug);
  //       break;
  //     case Logger::info:
  //       consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::info);
  //       break;
  //     case Logger::warning:
  //       consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::warning);
  //       break;
  //     case Logger::error:
  //       consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::error);
  //       break;
  //     case Logger::fatal:
  //       consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::fatal);
  //       break;
  //     default:
  //       consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::trace);
  //       std::cerr << "Unknown log level: " << level << std::endl;
  //       break;
  //   }
  // }
}

void Logger::LoggerImpl::setFileLogLevel(Logger::severity_level level) {
  // if (!fileSink_) {
  //   throw std::runtime_error("File sink is not initialized");
  // }

  // switch (level) {
  //   case Logger::trace:
  //     fileSink_->set_filter(logging::trivial::severity >= logging::trivial::trace);
  //     break;
  //   case Logger::debug:
  //     fileSink_->set_filter(logging::trivial::severity >= logging::trivial::debug);
  //     break;
  //   case Logger::info:
  //     fileSink_->set_filter(logging::trivial::severity >= logging::trivial::info);
  //     break;
  //   case Logger::warning:
  //     fileSink_->set_filter(logging::trivial::severity >= logging::trivial::warning);
  //     break;
  //   case Logger::error:
  //     fileSink_->set_filter(logging::trivial::severity >= logging::trivial::error);
  //     break;
  //   case Logger::fatal:
  //     fileSink_->set_filter(logging::trivial::severity >= logging::trivial::fatal);
  //     break;
  //   default:
  //     throw std::invalid_argument("Unknown log level");
  // }
}
