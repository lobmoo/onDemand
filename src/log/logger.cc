#include "logger.h"
#include "logger_impl.h"

Logger::Logger() : pImpl(std::make_unique<LoggerImpl>()) {}
Logger::~Logger() {}

Logger &Logger::Instance() {
    static Logger log;
    return log;
}

bool Logger::Init(const std::string& fileName, int type, int level, int maxFileSize, int maxBackupIndex, bool isAsync) {
    return pImpl->Init(fileName, type, level, maxFileSize, maxBackupIndex, isAsync);
}

void Logger::setConsoleLogLevel(const std::string &level) {
    pImpl->setConsoleLogLevel(level);
}

void Logger::setFileLogLevel(const std::string &level) {
    pImpl->setFileLogLevel(level);
}

void Logger::logTrace(const std::string& msg) const {
    pImpl->logTrace(msg);
}

void Logger::logDebug(const std::string& msg) const {
    pImpl->logDebug(msg);
}

void Logger::logInfo(const std::string& msg) const {
    pImpl->logInfo(msg);
}

void Logger::logWarning(const std::string& msg) const {
    pImpl->logWarning(msg);
}

void Logger::logError(const std::string& msg) const {
    pImpl->logError(msg);
}

void Logger::logFatal(const std::string& msg) const {
    pImpl->logFatal(msg);
}
// ... 其他日志宏定义 ...