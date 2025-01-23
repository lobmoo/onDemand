#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <string>

class Logger {
 public:
  enum LoggerType { both = 0, console, file };

  ~Logger();
  static Logger& Instance();
  bool Init(const std::string& fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync);
  void setConsoleLogLevel(const std::string& level);
  void setFileLogLevel(const std::string& level);

 public:
  void logTrace(const std::string& msg) const;
  void logDebug(const std::string& msg) const;
  void logInfo(const std::string& msg) const;
  void logWarning(const std::string& msg) const;
  void logError(const std::string& msg) const;
  void logFatal(const std::string& msg) const;

 private:
  class LoggerImpl;
  std::unique_ptr<LoggerImpl> pImpl;

  Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};

#define LOG_TRACE(msg)                \
  do {                                \
    Logger::Instance().logTrace(msg); \
  } while (0)

#define LOG_DEBUG(msg)                \
  do {                                \
    Logger::Instance().logDebug(msg); \
  } while (0)

#define LOG_INFO(msg)                \
  do {                               \
    Logger::Instance().logInfo(msg); \
  } while (0)

#define LOG_WARNING(msg)                \
  do {                                  \
    Logger::Instance().logWarning(msg); \
  } while (0)

#define LOG_ERROR(msg)                \
  do {                                \
    Logger::Instance().logError(msg); \
  } while (0)

#define LOG_FATAL(msg)                \
  do {                                \
    Logger::Instance().logFatal(msg); \
  } while (0)

#endif  // LOGGER_H