#include "logger.h"

#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

Logger &Logger::Instance() {
  static Logger log;
  return log;
}

boost::log::formatter formatter =
    boost::log::expressions::stream
    << "[" << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
    << "|" << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "]("
    << boost::log::expressions::attr<std::string>("File") << ":" << boost::log::expressions::attr<int>("Line") << ") "
    << " [" << boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") << "]"
    << boost::log::expressions::smessage;

boost::log::formatter color_formatter =
    boost::log::expressions::stream
    << "[" << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
    << "|" << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "]("
    << boost::log::expressions::attr<std::string>("File") << ":" << boost::log::expressions::attr<int>("Line") << ") "
    << boost::log::expressions::if_(
           boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") ==
           boost::log::trivial::debug)[boost::log::expressions::stream << BLUE]
    << boost::log::expressions::if_(
           boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") ==
           boost::log::trivial::info)[boost::log::expressions::stream << GREEN]
    << boost::log::expressions::if_(
           boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") ==
           boost::log::trivial::warning)[boost::log::expressions::stream << YELLOW]
    << boost::log::expressions::if_(
           boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") ==
           boost::log::trivial::error)[boost::log::expressions::stream << RED]
    << " [" << boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") << "]" << RESET << " "
    << boost::log::expressions::smessage;

void Logger::setconsoleSink() {
  consoleSink_ = boost::log::add_console_log();
  consoleSink_->set_formatter(color_formatter);
  boost::log::core::get()->add_sink(consoleSink_);
}

void Logger::setfileSink(std::string fileName, int maxFileSize, int maxBackupIndex) {
  boost::log::attributes::current_process_name process_name_attr;
  std::string pRocessName = process_name_attr.get();
  std::string filePattern = pRocessName + "_%Y%m%d_%H%M%S_%N.log";
  fileSink_ = boost::make_shared<file_sink>(
      boost::log::keywords::file_name = fileName, boost::log::keywords::target_file_name = filePattern,
      boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(16, 0, 0),
      boost::log::keywords::rotation_size = maxFileSize * 1024 * 1024,
      boost::log::keywords::open_mode = std::ios::out | std::ios::app);
  fileSink_->locked_backend()->set_file_collector(boost::log::sinks::file::make_collector(
      boost::log::keywords::target = "logs",
      boost::log::keywords::max_size = maxFileSize * maxBackupIndex * 1024 * 1024,
      boost::log::keywords::min_free_space = 10 * 1024 * 1024, boost::log::keywords::max_files = 512));

  fileSink_->set_formatter(formatter);
  fileSink_->locked_backend()->scan_for_files();
  fileSink_->locked_backend()->auto_flush(true);

  boost::log::core::get()->add_sink(fileSink_);
}

bool Logger::Init(std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  switch (type) {
    case both: {
      setconsoleSink();
      setfileSink(fileName, maxFileSize, maxBackupIndex);
    } break;
    case console: {
      setconsoleSink();
    } break;
    case file: {
      setfileSink(fileName, maxFileSize, maxBackupIndex);
    } break;
    default: {
    } break;
  }
  boost::log::add_common_attributes();
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= level);
  boost::log::core::get()->add_global_attribute("File", boost::log::attributes::mutable_constant<std::string>(""));
  boost::log::core::get()->add_global_attribute("Line", boost::log::attributes::mutable_constant<int>(0));
  return true;
}

void Logger::setConsoleLogLevel(const std::string &level) {
  if (level == "trace") {
    consoleSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
  } else if (level == "debug") {
    consoleSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
  } else if (level == "info") {
    consoleSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
  } else if (level == "warning") {
    consoleSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::warning);
  } else if (level == "error") {
    consoleSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::error);
  } else if (level == "fatal") {
    consoleSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::fatal);
  } else {
    std::cerr << "Unknown log level: " << level << std::endl;
  }
}

void Logger::setFileLogLevel(const std::string &level) {
  if (!fileSink_) {
    throw std::runtime_error("File sink is not initialized");
  }
  if (level == "trace") {
    fileSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
  } else if (level == "debug") {
    fileSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
  } else if (level == "info") {
    fileSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
  } else if (level == "warning") {
    fileSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::warning);
  } else if (level == "error") {
    fileSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::error);
  } else if (level == "fatal") {
    fileSink_->set_filter(boost::log::trivial::severity >= boost::log::trivial::fatal);
  } else {
    throw std::invalid_argument("Unknown log level: " + level);
  }
}
