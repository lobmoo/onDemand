#include "logger_impl.h"
#include <iostream>
#include <ctime>
#include "logger.h"

LoggerImpl::LoggerImpl() : logLevel_(DEBUG) {}

LoggerImpl::~LoggerImpl() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void LoggerImpl::setLevel(int level) {
    logLevel_ = level;
}

void LoggerImpl::setOutputFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }
    logFile_.open(filename, std::ios::out | std::ios::app);
}

void LoggerImpl::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.flush();
    }
}

void LoggerImpl::log(const std::string& level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 生成时间戳
    std::time_t now = std::time(nullptr);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::string logMsg = std::string("[") + buf + "] [" + level + "] " + msg + "\n";

    if (logFile_.is_open()) {
        logFile_ << logMsg;
    } else {
        std::cout << logMsg;
    }
}

void LoggerImpl::debug(const std::string& msg) {
    if (logLevel_ <= DEBUG) log("DEBUG", msg);
}

void LoggerImpl::info(const std::string& msg) {
    if (logLevel_ <= INFO) log("INFO", msg);
}

void LoggerImpl::warn(const std::string& msg) {
    if (logLevel_ <= WARN) log("WARN", msg);
}

void LoggerImpl::error(const std::string& msg) {
    if (logLevel_ <= ERROR) log("ERROR", msg);
}
