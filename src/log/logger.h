#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

class Logger {
 public:
  enum severity_level { trace, debug, info, warning, error, critical };
  enum LoggerType { both = 0, console, file };

 public:
  ~Logger();
  static Logger& Instance();
  bool Init(
      const std::string& fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync = false);
  void Log(severity_level level, const std::string& msg, const char* file, int line, const char* func);
  void Uinit();

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
  const char* func_;
  int line_;
  std::ostringstream stream_;
};

#define LOG(level) LogStream(Logger::Instance(), Logger::level, __FILE__, __LINE__, __FUNCTION__).stream()

class LogRateLimiter {
 public:
  static bool shouldLog(const std::string& key, int interval_ms) {
    using namespace std::chrono;
    thread_local static std::unordered_map<std::string, steady_clock::time_point> last_log_times;

    auto now = steady_clock::now();
    auto it = last_log_times.find(key);
    if (it == last_log_times.end() || duration_cast<milliseconds>(now - it->second).count() >= interval_ms) {
      last_log_times[key] = now;
      return true;
    }
    return false;
  }
};

#define LOG_TIME(level, interval_ms) \
  if (LogRateLimiter::shouldLog(std::string(__FILE__) + ":" + std::to_string(__LINE__), interval_ms)) LOG(level)

#endif  // LOGGER_H