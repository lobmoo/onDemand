/**
 * @file logger_impl.cc
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-03-25
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-03-25     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#include "logger_impl.h"

#include <iomanip>
#include <memory>
#include <sstream>


Logger::LoggerImpl::LoggerImpl() : flushEvery_(0), flushOnLevel_(spdlog::level::err)
{
}
Logger::LoggerImpl::~LoggerImpl()
{
}

bool Logger::LoggerImpl::Init(const std::string logConfigFilePath)
{
    
    LoggerConfig  Config();

    // int type = loggerConfig.getType(loggerConfig.type);
    // int level = loggerConfig.getLevel(loggerConfig.level);
    // if (!Init(loggerConfig.fileName, type, level, loggerConfig.maxFileSize,
    //           loggerConfig.maxBackupIndex, loggerConfig.isAsync)) {
    //     return false;
    // }

    // setLogLevel(loggerConfig.getLevel(loggerConfig.level));
    // setFlushEvery(loggerConfig.flushEvery);
    // setFlushOnLevel(loggerConfig.getLevel(loggerConfig.flushOnLevel));
    // setLogConsoleLevel(loggerConfig.getLevel(loggerConfig.LogConsoleLevel));
    // setLogFileLevel(loggerConfig.getLevel(loggerConfig.LogFileLevel));
    // setLogPattern(loggerConfig.LogPattern);
    return true;
}

bool Logger::LoggerImpl::Init(std::string fileName, int type, int level, int maxFileSize,
                              int maxBackupIndex, bool isAsync)
{
    std::size_t max_file_size = 1024 * 1024 * maxFileSize;
    std::vector<spdlog::sink_ptr> sinks;

    spdlog::level::level_enum console_level =
        GetLogLevelFromEnv("LOG_CONSOLE_LEVEL", static_cast<spdlog::level::level_enum>(level));
    spdlog::level::level_enum file_level =
        GetLogLevelFromEnv("LOG_FILE_LEVEL", static_cast<spdlog::level::level_enum>(level));

    switch (type) {
        case Logger::both: {
            console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            file_sink_ = std::make_shared<spdlog::sinks::custom_rotating_file_sink_mt>(
                fileName, max_file_size, maxBackupIndex);
            /*按天数轮转记录器  todo*/
            // file_sink_ = std::make_shared<spdlog::sinks::daily_file_sink_mt>(fileName, 23, 59);
            console_sink_->set_level(console_level);
            file_sink_->set_level(file_level);
            sinks.push_back(file_sink_);
            sinks.push_back(console_sink_);
        } break;
        case Logger::console: {
            console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink_->set_level(console_level);
            sinks.push_back(console_sink_);
        } break;
        case Logger::file: {
            file_sink_ = std::make_shared<spdlog::sinks::custom_rotating_file_sink_mt>(
                fileName, max_file_size, maxBackupIndex);
            file_sink_->set_level(file_level);
            sinks.push_back(file_sink_);
        } break;
        default:
            console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink_->set_level(console_level);
            sinks.push_back(console_sink_);
            break;
    }
    if (sinks.empty()) {
        spdlog::error("No sinks were created for the logger");
        return false;
    }
    if (isAsync) {
        spdlog::init_thread_pool(logBufferSize_, std::thread::hardware_concurrency() / 4);
        logger_ = std::make_shared<spdlog::async_logger>("Logger", sinks.begin(), sinks.end(),
                                                         spdlog::thread_pool(),
                                                         spdlog::async_overflow_policy::block);
    } else {
        logger_ = std::make_shared<spdlog::logger>("Logger", sinks.begin(), sinks.end());
    }
    logger_->set_level(spdlog::level::trace);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%# %!] %v");
    logger_->flush_on(flushOnLevel_);

    if (flushEvery_ > 0) {
        spdlog::flush_every(std::chrono::seconds(flushEvery_));
    }

    spdlog::register_logger(logger_);
    spdlog::set_default_logger(logger_);
    return true;
}

void Logger::LoggerImpl::Uinit()
{
    if (logger_) {
        logger_->flush();
        spdlog::drop("Logger");
        logger_.reset();
    }
    spdlog::shutdown();
}

void Logger::LoggerImpl::log(Logger::severity_level level, const std::string &msg, const char *file,
                             int line, const char *func)
{
    if (logger_) {
        logger_->log(spdlog::source_loc{file, line, func},
                     static_cast<spdlog::level::level_enum>(level), msg);
    }
}

spdlog::level::level_enum
Logger::LoggerImpl::GetLogLevelFromEnv(const std::string &env_var,
                                       spdlog::level::level_enum default_level)
{
    const char *env_value = std::getenv(env_var.c_str());
    if (!env_value) {
        return default_level;
    }
    std::string level_str(env_value);
    return spdlog::level::from_str(level_str);
}

void Logger::LoggerImpl::setFlushEvery(uint32_t flushEvery)
{
    if (flushEvery > 0) {
        flushEvery_ = flushEvery;
    }
}

void Logger::LoggerImpl::setFlushOnLevel(Logger::severity_level flushOnLevel)
{
    flushOnLevel_ = static_cast<spdlog::level::level_enum>(flushOnLevel);
}

void Logger::LoggerImpl::setLogLevel(Logger::severity_level level)
{
    if (logger_) {
        logger_->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

void Logger::LoggerImpl::setLogPattern(const std::string &pattern)
{
    if (logger_) {
        logger_->set_pattern(pattern);
    }
}

void Logger::LoggerImpl::setLogConsoleLevel(Logger::severity_level level)
{
    if (console_sink_) {
        console_sink_->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

void Logger::LoggerImpl::setLogFileLevel(Logger::severity_level level)
{
    if (file_sink_) {
        file_sink_->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

void Logger::LoggerImpl::setLogBufferSize(size_t size)
{
    logBufferSize_ = size;
}