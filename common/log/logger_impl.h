/**
 * @file logger_impl.h
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
#ifndef LOGGER_IMPL_H
#define LOGGER_IMPL_H

#include <spdlog/async.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <atomic>

#include "custom_rotating_file_sink.h"
#include "logger.h"

using json = nlohmann::json;
class Logger::LoggerImpl
{
    struct LoggerConfig {
        explicit LoggerConfig(const std::string &jsonPath)
            : fileName("./"), type("console"), level("trace"), maxFileSize(60), maxBackupIndex(5),
              isAsync(false), flushOnLevel("error"), LogConsoleLevel("info"), LogFileLevel("info"),
              LogPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%# %!] %v"), isValid(true)
        {
            auto WARNINGMESSAGE =
                [](const std::string &message){
                    spdlog::warn("\n{}\n[WARNING] {}\n{}\n", std::string(80, '!'), message,
                                 std::string(80, '!'));
                    };

            std::ifstream inFile(jsonPath);
            if (!inFile.is_open()) {
                WARNINGMESSAGE("Failed to open config file: " + jsonPath + ", using default config");
                isValid = false;
                return; // 文件打不开 → 用默认值
            }

            json j;
            json loggerConfig;

            try {
                inFile >> j;
                loggerConfig = j.value("LoggerConfig", json::object());

                fileName = loggerConfig.value("FileName", fileName);
                type = loggerConfig.value("LogType", type);
                level = loggerConfig.value("LogLevel", level);
                maxFileSize = loggerConfig.value("MaxFileSize", maxFileSize);
                maxBackupIndex = loggerConfig.value("MaxBackupIndex", maxBackupIndex);
                isAsync = loggerConfig.value("IsAsync", isAsync);
                flushOnLevel = loggerConfig.value("FlushOnLevel", flushOnLevel);
                LogConsoleLevel = loggerConfig.value("ConsoleLogLevel", LogConsoleLevel);
                LogFileLevel = loggerConfig.value("FileLogLevel", LogFileLevel);
                LogPattern = loggerConfig.value("LogPattern", LogPattern);

            } catch (const json::exception &e) {
                spdlog::warn("Failed to parse JSON config: {}, using default config", e.what());
                isValid = false;
                return;
            }
        }

        LoggerType getType() const
        {
            if (type == "console") {
                return Logger::console;
            } else if (type == "file") {
                return Logger::file;
            } else if (type == "both") {
                return Logger::both;
            } else {
                return Logger::console; // 默认值
            }
        }
        severity_level getLevel(const std::string &levelStr) const
        {
            if (levelStr == "trace") {
                return Logger::trace;
            } else if (levelStr == "debug") {
                return Logger::debug;
            } else if (levelStr == "info") {
                return Logger::info;
            } else if (levelStr == "warning") {
                return Logger::warning;
            } else if (levelStr == "error") {
                return Logger::error;
            } else if (levelStr == "critical") {
                return Logger::critical;
            } else {
                return Logger::info; // 默认值
            }
        }
        severity_level getLogLevel() const { return getLevel(level); }
        severity_level getFlushOnLevel() const { return getLevel(flushOnLevel); }
        severity_level getConsoleLogLevel() const { return getLevel(LogConsoleLevel); }
        severity_level getFileLogLevel() const { return getLevel(LogFileLevel); }
        std::string getLogPattern() const { return LogPattern; }
        std::string getFileName() const { return fileName; }
        bool getIsAsync() const { return isAsync; }
        uint32_t getMaxFileSize() const { return maxFileSize; }
        uint32_t getMaxBackupIndex() const { return maxBackupIndex; }
        bool getIsValid() const { return isValid; }

    private:
        std::string fileName;
        std::string type;
        std::string level;
        uint32_t maxFileSize;
        uint32_t maxBackupIndex;
        bool isAsync;
        std::string flushOnLevel;
        std::string LogConsoleLevel;
        std::string LogFileLevel;
        std::string LogPattern;
        bool isValid;
    };

public:
    LoggerImpl();
    ~LoggerImpl();

    bool Init(std::string fileName, LoggerType type, severity_level level, uint32_t maxFileSize,
              uint32_t maxBackupIndex, bool isAsync);
    bool Init(const std::string logConfigFilePath);

    void log(Logger::severity_level level, const std::string &msg, const char *file, uint32_t line,
             const char *func);
    void Uinit();

    void setFlushEvery(uint32_t flushEvery);
    void setFlushOnLevel(Logger::severity_level flushOnLevel);
    void setLogLevel(Logger::severity_level level);
    void setLogPattern(const std::string &pattern);
    void setLogConsoleLevel(Logger::severity_level level);
    void setLogFileLevel(Logger::severity_level level);
    void setLogBufferSize(size_t size);

private:
    std::shared_ptr<spdlog::logger> logger_ = nullptr;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_ = nullptr;
    std::shared_ptr<spdlog::sinks::custom_rotating_file_sink_mt> file_sink_ = nullptr;
    uint32_t flushEvery_;
    size_t logBufferSize_;
    spdlog::level::level_enum flushOnLevel_;
    std::atomic<bool> isRunning_;
    std::string logConfigFilePath_;

private:
    spdlog::level::level_enum GetLogLevelFromEnv(const std::string &env_var,
                                                 spdlog::level::level_enum default_level);
    void LoggerConfigChecker();
    void stop();
    void LogApplyConfig(const LoggerConfig &config);
};

#endif // LOGGER_IMPL_H