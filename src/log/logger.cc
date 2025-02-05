#include "logger.h"

#include "logger_impl.h"

Logger::Logger() : pImpl(std::make_unique<LoggerImpl>()) {}
Logger::~Logger() {}

Logger& Logger::Instance() {
  static Logger log;
  return log;
}

void Logger::logInternal(Logger::severity_level level, const std::string& msg, const char* file, int line) {
  pImpl->log(level, msg, file, line);
}

bool Logger::Init(const std::string& fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  return pImpl->Init(fileName, type, level, maxFileSize, maxBackupIndex, isAsync);
}

void Logger::setConsoleLogLevel(const Logger::severity_level level) { pImpl->setConsoleLogLevel(level); }

void Logger::setFileLogLevel(const Logger::severity_level level) { pImpl->setFileLogLevel(level); }

Logger::LogStream::~LogStream() { Logger::Instance().logInternal(level_, stream_.str(), file_, line_); }
