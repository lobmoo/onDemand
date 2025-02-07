#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <string>
#include <sstream>
#include <mutex>

class Logger {
 public:
  enum severity_level { trace, debug, info, warning, error, critical };
  enum LoggerType { both = 0, console, file };

  class LogStream {
   public:
    LogStream(severity_level level, const char* file, int line) : level_(level), file_(file), line_(line) {}

    template <typename T>
    LogStream& operator<<(const T& value) {
      stream_ << value;
      return *this;
    }
    ~LogStream();

   private:
    severity_level level_;
    std::ostringstream stream_;
    const char* file_;
    int line_;
  };

 public:
  ~Logger();
  static Logger& Instance();

  bool Init(const std::string& fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync);
  void setConsoleLogLevel(const Logger::severity_level level);
  void setFileLogLevel(const Logger::severity_level level);

 private:
  class LoggerImpl;
  std::unique_ptr<LoggerImpl> pImpl;
  Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  void logInternal(severity_level level, const std::string& msg, const char* file, int line);
  friend class LogStream;
};



#define LOG_TRACE Logger::LogStream(Logger::trace, __FILE__, __LINE__)
#define LOG_DEBUG Logger::LogStream(Logger::debug, __FILE__, __LINE__)
#define LOG_INFO Logger::LogStream(Logger::info, __FILE__, __LINE__)
#define LOG_WARNING Logger::LogStream(Logger::warning, __FILE__, __LINE__)
#define LOG_ERROR Logger::LogStream(Logger::error, __FILE__, __LINE__)
#define LOG_FATAL Logger::LogStream(Logger::fatal, __FILE__, __LINE__)

#define LOG(LEVEL) Logger::LogStream(Logger::severity_level::LEVEL, __FILE__, __LINE__)
#endif  // LOGGER_H