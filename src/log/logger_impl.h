#ifndef LOGGER_IMPL_H
#define LOGGER_IMPL_H

#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "logger.h"

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

// 在这里定义日志宏
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

#define LOG_EXTRA_INFO                                                               \
  boost::log::attribute_cast<boost::log::attributes::mutable_constant<std::string>>( \
      boost::log::core::get()->get_global_attributes()["File"])                      \
      .set(__FILE__);                                                                \
  boost::log::attribute_cast<boost::log::attributes::mutable_constant<int>>(         \
      boost::log::core::get()->get_global_attributes()["Line"])                      \
      .set(__LINE__);

class Logger::LoggerImpl {
 public:
  typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> file_sink;
  typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_file_backend> async_file_sink;
  typedef boost::log::v2s_mt_posix::sinks::synchronous_sink<boost::log::v2s_mt_posix::sinks::text_ostream_backend>
      console_sink;
  typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_ostream_backend> async_console_sink;

  LoggerImpl();
  ~LoggerImpl();

  bool Init(std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync);
  void setConsoleLogLevel(const std::string& level);
  void setFileLogLevel(const std::string& level);

 public:
  void logTrace(const std::string& msg);
  void logDebug(const std::string& msg);
  void logInfo(const std::string& msg);
  void logWarning(const std::string& msg);
  void logError(const std::string& msg);
  void logFatal(const std::string& msg);

 private:
  void setconsoleSink();
  void setfileSink(std::string fileName, int maxFileSize, int maxBackupIndex);
  src::severity_logger<logging::trivial::severity_level> _logger;
  logging::formatter formatter;
  logging::formatter color_formatter;
  boost::shared_ptr<console_sink> consoleSink_;
  boost::shared_ptr<file_sink> fileSink_;

 public:
};

#endif  // LOGGER_IMPL_H