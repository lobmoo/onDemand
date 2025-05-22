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

#include "custom_rotating_file_sink.h"
#include "logger.h"

using json = nlohmann::json;
class Logger::LoggerImpl
{
    struct LoggerConfig {
        explicit LoggerConfig(const std::string &jsonPath)
            : fileName("./"), type("console"), level("info"), maxFileSize(60), maxBackupIndex(5),
              isAsync(false), flushEvery(1), flushOnLevel("error"), LogConsoleLevel("info"),
              LogFileLevel("info"), LogPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%# %!] %v")
        {
            std::ifstream inFile(jsonPath);
            if (!inFile.is_open()) {
                spdlog::warn("Failed to open config file: {}, using default config", jsonPath);
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
                flushEvery = loggerConfig.value("FlushEvery", flushEvery);
                flushOnLevel = loggerConfig.value("FlushOnLevel", flushOnLevel);
                LogConsoleLevel = loggerConfig.value("ConsoleLogLevel", LogConsoleLevel);
                LogFileLevel = loggerConfig.value("FileLogLevel", LogFileLevel);
                LogPattern = loggerConfig.value("LogPattern", LogPattern);

            } catch (const json::exception &e) {
                spdlog::warn("Failed to parse JSON config: {}, using default config", e.what());
                // 不要 throw，程序继续运行
                return;
            }
        }

        std::string fileName;
        std::string type;
        std::string level;
        int maxFileSize;
        int maxBackupIndex;
        bool isAsync;
        int flushEvery;
        std::string flushOnLevel;
        std::string LogConsoleLevel;
        std::string LogFileLevel;
        std::string LogPattern;
    };

public:
    LoggerImpl();
    ~LoggerImpl();

    bool Init(std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex,
              bool isAsync);
    bool Init(const std::string logConfigFilePath);

    void log(Logger::severity_level level, const std::string &msg, const char *file, int line,
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

private:
    spdlog::level::level_enum GetLogLevelFromEnv(const std::string &env_var,
                                                 spdlog::level::level_enum default_level);
};

#endif // LOGGER_IMPL_H