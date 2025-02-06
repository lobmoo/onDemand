#include "logger.h"
#include "logger_impl.h"

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : pimpl_(std::make_unique<LoggerImpl>()) {}
Logger::~Logger() = default;

void Logger::debug(const std::string& msg) { pimpl_->debug(msg); }
void Logger::info(const std::string& msg) { pimpl_->info(msg); }
void Logger::warn(const std::string& msg) { pimpl_->warn(msg); }
void Logger::error(const std::string& msg) { pimpl_->error(msg); }

void Logger::setLevel(int level) { pimpl_->setLevel(level); }
void Logger::setOutputFile(const std::string& filename) { pimpl_->setOutputFile(filename); }
void Logger::flush() { pimpl_->flush(); }

// 日志流实现
LoggerStream::LoggerStream(LogLevel level, const char* file, int line) : level_(level) {
    stream_ << "[" << file << ":" << line << "] ";
}

LoggerStream::~LoggerStream() {
    switch (level_) {
        case DEBUG: Logger::getInstance().debug(stream_.str()); break;
        case INFO:  Logger::getInstance().info(stream_.str()); break;
        case WARN:  Logger::getInstance().warn(stream_.str()); break;
        case ERROR: Logger::getInstance().error(stream_.str()); break;
    }
}
