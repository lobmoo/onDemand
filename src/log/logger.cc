#include "logger.h"

#include "logger_impl.h"

Logger::Logger() : pImpl(std::make_unique<LoggerImpl>()) {}
Logger::~Logger() {}

Logger& Logger::Instance() {
  static Logger log;
  return log;
}

void Logger::Uinit()
{
   return pImpl->Uinit();
}

bool Logger::Init(const std::string& fileName, LoggerType type, severity_level level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  return pImpl->Init(fileName, type, level, maxFileSize, maxBackupIndex, isAsync);
}

void Logger::Log(severity_level level, const std::string& msg, const char* file, int line, const char* func) {
  pImpl->log(level, msg, file, line, func);
}


void Logger::setFlushEvery(uint32_t flushEvery)
{
    pImpl->setFlushEvery(flushEvery);
}

void Logger::setFlushOnLevel(Logger::severity_level flushOnLevel)
{
    pImpl->setFlushOnLevel(flushOnLevel);
}