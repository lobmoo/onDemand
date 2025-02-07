#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <mutex>
#include <sstream>
#include <string>

class Logger {
 public:
  enum severity_level { trace, debug, info, warning, error, critical };
  enum LoggerType { both = 0, console, file };

 public:
  ~Logger();
  static Logger& Instance();
  bool Init(const std::string& fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync = false);
  void Log(severity_level level, const std::string& msg, const char* file, int line, const char* func); 
  void setConsoleLogLevel(const Logger::severity_level level);
  void setFileLogLevel(const Logger::severity_level level);

 private:
  class LoggerImpl;
  std::unique_ptr<LoggerImpl> pImpl;
  Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};

class LogStream {
 public:
  LogStream(Logger& logger, Logger::severity_level level, const char* file, int line, const char* func)
      : logger_(logger), level_(level), file_(file), line_(line), func_(func) {}
  ~LogStream() { logger_.Log(level_, stream_.str(), file_, line_, func_); }

  std::ostringstream& stream() { return stream_; }

 private:
  Logger& logger_;
  Logger::severity_level level_;
  const char* file_;
  const char *func_;
  int line_;
  std::ostringstream stream_;
};

#define LOG(level) LogStream(Logger::Instance(), Logger::level, __FILE__, __LINE__, __FUNCTION__).stream()

#endif  // LOGGER_H