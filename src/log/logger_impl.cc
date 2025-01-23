#include "logger_impl.h"

Logger::LoggerImpl::LoggerImpl()
    : formatter(
          expr::stream << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                       << "|" << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "]("
                       << expr::attr<std::string>("File") << ":" << expr::attr<int>("Line") << ") "
                       << " [" << expr::attr<boost::log::trivial::severity_level>("Severity") << "]" << expr::smessage),
      color_formatter(
          expr::stream << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                       << "|" << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "]("
                       << expr::attr<std::string>("File") << ":" << expr::attr<int>("Line") << ") "
                       << expr::if_(
                              expr::attr<boost::log::trivial::severity_level>("Severity") ==
                              boost::log::trivial::debug)[expr::stream << BLUE]
                       << expr::if_(
                              expr::attr<boost::log::trivial::severity_level>("Severity") ==
                              boost::log::trivial::info)[expr::stream << GREEN]
                       << expr::if_(
                              expr::attr<boost::log::trivial::severity_level>("Severity") ==
                              boost::log::trivial::warning)[expr::stream << YELLOW]
                       << expr::if_(
                              expr::attr<boost::log::trivial::severity_level>("Severity") ==
                              boost::log::trivial::error)[expr::stream << RED]
                       << " [" << expr::attr<boost::log::trivial::severity_level>("Severity") << "]" << RESET << " "
                       << expr::smessage) {}

Logger::LoggerImpl::~LoggerImpl() {
  if (consoleSink_) logging::core::get()->remove_sink(consoleSink_);
  if (fileSink_) logging::core::get()->remove_sink(fileSink_);
}

bool Logger::LoggerImpl::Init(
    std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  switch (type) {
    case Logger::both: {
      if (!isAsync) {
        setconsoleSink();
        setfileSink(fileName, maxFileSize, maxBackupIndex);
      }
    } break;
    case Logger::console: {
      if (!isAsync) {
        setconsoleSink();
      }
    } break;
    case Logger::file: {
      if (!isAsync) {
        setfileSink(fileName, maxFileSize, maxBackupIndex);
      }
    } break;
    default:
      break;
  }

  logging::add_common_attributes();
  logging::core::get()->set_filter(logging::trivial::severity >= level);
  logging::core::get()->add_global_attribute("File", logging::attributes::mutable_constant<std::string>(""));
  logging::core::get()->add_global_attribute("Line", logging::attributes::mutable_constant<int>(0));

  return true;
}

void Logger::LoggerImpl::setconsoleSink() {
  consoleSink_ = boost::log::add_console_log();
  consoleSink_->set_formatter(color_formatter);
  boost::log::core::get()->add_sink(consoleSink_);
}

void Logger::LoggerImpl::setfileSink(std::string fileName, int maxFileSize, int maxBackupIndex) {
  boost::log::attributes::current_process_name process_name_attr;
  std::string pRocessName = process_name_attr.get();
  std::string filePattern = pRocessName + "_%Y%m%d_%H%M%S_%N.log";

  fileSink_ = boost::make_shared<file_sink>(
      keywords::file_name = fileName, keywords::target_file_name = filePattern,
      keywords::rotation_size = maxFileSize * 1024 * 1024,
      keywords::time_based_rotation = sinks::file::rotation_at_time_point(16, 0, 0),
      keywords::open_mode = std::ios::out | std::ios::app);

  fileSink_->locked_backend()->set_file_collector(sinks::file::make_collector(
      keywords::target = "logs", keywords::max_size = maxFileSize * maxBackupIndex * 1024 * 1024,
      keywords::min_free_space = 10 * 1024 * 1024, keywords::max_files = 512));

  fileSink_->set_formatter(formatter);
  fileSink_->locked_backend()->scan_for_files();
  fileSink_->locked_backend()->auto_flush(true);
  logging::core::get()->add_sink(fileSink_);
}

void Logger::LoggerImpl::setConsoleLogLevel(const std::string &level) {
  if (consoleSink_) {
    if (level == "trace") {
      consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::trace);
    } else if (level == "debug") {
      consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::debug);
    } else if (level == "info") {
      consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::info);
    } else if (level == "warning") {
      consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::warning);
    } else if (level == "error") {
      consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::error);
    } else if (level == "fatal") {
      consoleSink_->set_filter(logging::trivial::severity >= logging::trivial::fatal);
    } else {
      std::cerr << "Unknown log level: " << level << std::endl;
    }
  }
}

void Logger::LoggerImpl::setFileLogLevel(const std::string &level) {
  if (!fileSink_) {
    throw std::runtime_error("File sink is not initialized");
  }
  if (level == "trace") {
    fileSink_->set_filter(logging::trivial::severity >= logging::trivial::trace);
  } else if (level == "debug") {
    fileSink_->set_filter(logging::trivial::severity >= logging::trivial::debug);
  } else if (level == "info") {
    fileSink_->set_filter(logging::trivial::severity >= logging::trivial::info);
  } else if (level == "warning") {
    fileSink_->set_filter(logging::trivial::severity >= logging::trivial::warning);
  } else if (level == "error") {
    fileSink_->set_filter(logging::trivial::severity >= logging::trivial::error);
  } else if (level == "fatal") {
    fileSink_->set_filter(logging::trivial::severity >= logging::trivial::fatal);
  } else {
    throw std::invalid_argument("Unknown log level: " + level);
  }
}

void Logger::LoggerImpl::logTrace(const std::string& msg) {
    BOOST_LOG_FUNCTION();
    LOG_EXTRA_INFO;
    BOOST_LOG_SEV(_logger, boost::log::trivial::trace) << msg;
}

void Logger::LoggerImpl::logDebug(const std::string& msg) {
    BOOST_LOG_FUNCTION();
    LOG_EXTRA_INFO;
    BOOST_LOG_SEV(_logger, boost::log::trivial::debug) << msg;
}

void Logger::LoggerImpl::logInfo(const std::string& msg) {
    BOOST_LOG_FUNCTION();
    LOG_EXTRA_INFO;
    BOOST_LOG_SEV(_logger, boost::log::trivial::info) << msg;
}

void Logger::LoggerImpl::logWarning(const std::string& msg) {
    BOOST_LOG_FUNCTION();
    LOG_EXTRA_INFO;
    BOOST_LOG_SEV(_logger, boost::log::trivial::warning) << msg;
}

void Logger::LoggerImpl::logError(const std::string& msg) {
    BOOST_LOG_FUNCTION();
    LOG_EXTRA_INFO;
    BOOST_LOG_SEV(_logger, boost::log::trivial::error) << msg;
}

void Logger::LoggerImpl::logFatal(const std::string& msg) {
    BOOST_LOG_FUNCTION();
    LOG_EXTRA_INFO;
    BOOST_LOG_SEV(_logger, boost::log::trivial::fatal) << msg;
}
