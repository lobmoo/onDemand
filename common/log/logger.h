/**
 * @file logger.h
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

#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

class Logger
{
public:
    enum severity_level { trace, debug, info, warning, error, critical };
    enum LoggerType { both = 0, console, file };

public:
    ~Logger() = default;

    /**
   * @brief 初始化日志系统
   *
   * @param fileName        日志文件名，支持绝对路径和相对路径
   * @param type           日志类型，可选值：both（文件和控制台）, console（控制台）, file（文件）
   * @param level          日志级别，控制日志的输出级别
   * @param maxFileSize    单个日志文件的最大大小（单位：MB）
   * @param maxBackupIndex 备份日志文件的最大数量
   * @param isAsync        是否启用异步日志，默认为 false（同步模式）  
   * @return true          初始化成功
   * @return false         初始化失败
   */
    bool Init(const std::string &fileName, LoggerType type, severity_level level,
              uint32_t maxFileSize, uint32_t maxBackupIndex, bool isAsync = false);
    /**
     * @brief 根据配置文件生成日志器。
     *
     * - 当日志路径无效时，将启用默认配置运行，此时不会写入文件。
     * - 默认参数如下：
     *     type            : "console"
     *     level           : "trace"
     *     maxFileSize     : 60 (MB)
     *     maxBackupIndex  : 5
     *     isAsync         : false
     *     flushOnLevel    : "error"
     *     LogConsoleLevel : "info"
     *     LogFileLevel    : "info"
     *     LogPattern      : "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%# %!] %v"
     *     isValid         : true
     *
     * @param logConfigFilePath 配置文件路径
     * @return true  加载成功
     * @return false 加载失败，使用默认配置
     */
    bool Init(const std::string &logConfigFilePath);

    /**
   * @brief 注销日志实例
   */
    void Uinit();

    /**
   * @brief Set the Flush Every object  设置日志刷入文件的频率，单位：秒 默认0 则按默认机制刷入
   * @param  flushEvery       间隔时间
   */
    void setFlushEvery(uint32_t flushEvery);

    /**
   * @brief Set the Flush On Level object  设置日志立即刷入文件的级别 默认为error以上立即刷入 
   * @param  flushOnLevel     级别
   */
    void setFlushOnLevel(Logger::severity_level flushOnLevel);

    /**
     * @brief Set the Log Level object 动态设置日志输出级别
     * @param  level           级别
     */
    void setLogLevel(Logger::severity_level level);

    /**
     * @brief Set the Log Pattern object 设置日志输出格式
     * @param  pattern         格式
     */
    void setLogPattern(const std::string &pattern);

    /**
     * @brief Set the Log Console Level object 设置控制台日志输出级别
     * @param  level           级别
     * @return * void 
     */
    void setLogConsoleLevel(Logger::severity_level level);

    /**
     * @brief Set the Log File Level object 设置文件日志输出级别
     * @param  level           级别
     * @return * void 
     */
    void setLogFileLevel(Logger::severity_level level);

    /**
     * @brief Set the Log Buffer Size object 设置日志缓冲区大小，仅在异步模式下有效  默认 8 * 1024 字节
     * @param  size            大小，单位：字节
     * @return * void 
     */
    void setLogBufferSize(size_t size);

    static Logger *Instance();

private:
    void Log(severity_level level, const std::string &msg, const char *file, uint32_t line,
             const char *func);
    class LoggerImpl;
    std::unique_ptr<LoggerImpl> pImpl;
    Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    friend class LogStream;
};

class LogStream
{
public:
    LogStream(Logger &logger, Logger::severity_level level, const char *file, uint32_t line,
              const char *func)
        : logger_(logger), level_(level), file_(file), line_(line), func_(func)
    {
    }
    ~LogStream() { logger_.Log(level_, stream_.str(), file_, line_, func_); }

    std::ostringstream &stream() { return stream_; }

private:
    Logger &logger_;
    Logger::severity_level level_;
    const char *file_;
    const char *func_;
    uint32_t line_;
    std::ostringstream stream_;
};

class LogRateLimiter
{
public:
    static bool shouldLog(const std::string &key, uint32_t interval_ms)
    {
        using namespace std::chrono;
        thread_local static std::unordered_map<std::string, steady_clock::time_point>
            last_log_times;

        auto now = steady_clock::now();
        auto it = last_log_times.find(key);
        if (it == last_log_times.end()
            || duration_cast<milliseconds>(now - it->second).count() >= interval_ms) {
            last_log_times[key] = now;
            return true;
        }
        return false;
    }
};

#define LOG(level)                                                                                 \
    LogStream(*Logger::Instance(), Logger::level, __FILE__, __LINE__, __FUNCTION__).stream()
#define LOG_TIME(level, interval_ms)                                                               \
    if (LogRateLimiter::shouldLog(std::string(__FILE__) + ":" + std::to_string(__LINE__),          \
                                  interval_ms))                                                    \
    LOG(level)

#endif // LOGGER_H