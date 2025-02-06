#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <string>
#include <sstream>

class LoggerImpl;  // 前向声明

class Logger {
public:
    static Logger& getInstance(); // 单例模式

    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

    void setLevel(int level);
    void setOutputFile(const std::string& filename);
    void flush();

private:
    Logger();  // 私有构造
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::unique_ptr<LoggerImpl> pimpl_;
};

// 定义日志级别
enum LogLevel { DEBUG, INFO, WARN, ERROR };

// 定义日志宏
#define LOG(level) LoggerStream(level, __FILE__, __LINE__)

class LoggerStream {
public:
    LoggerStream(LogLevel level, const char* file, int line);
    ~LoggerStream();

    template <typename T>
    LoggerStream& operator<<(const T& msg) {
        stream_ << msg;
        return *this;
    }

private:
    std::ostringstream stream_;
    LogLevel level_;
};

#endif  // LOGGER_H
