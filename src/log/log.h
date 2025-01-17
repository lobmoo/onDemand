#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/timer.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/support/date_time.hpp>
#include <string>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

// 日志级别封装
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    // 初始化日志系统
    void init(const std::string& logFile = "app.log", LogLevel level = LogLevel::Info) {
        // 设置全局日志格式
        logging::core::get()->set_filter(getSeverityFilter(level));

        // 设置日志文件输出
        logging::add_file_log(
            keywords::file_name = logFile,                  // 日志文件名
            keywords::rotation_size = 10 * 1024 * 1024,     // 文件滚动大小 (10 MB)
            keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(0, 0, 0), // 每天滚动
            keywords::format = expr::stream
                << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S") << "] "
                << "<" << logging::trivial::severity << "> "
                << expr::smessage
        );

        // 设置控制台输出
        logging::add_console_log(
            std::cout,
            keywords::format = expr::stream
                << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S") << "] "
                << "<" << logging::trivial::severity << "> "
                << expr::smessage
        );

        // 添加通用属性
        logging::add_common_attributes();
    }

    // 日志输出
    void log(LogLevel level, const std::string& message) {
        switch (level) {
            case LogLevel::Trace: BOOST_LOG_TRIVIAL(trace) << message; break;
            case LogLevel::Debug: BOOST_LOG_TRIVIAL(debug) << message; break;
            case LogLevel::Info: BOOST_LOG_TRIVIAL(info) << message; break;
            case LogLevel::Warning: BOOST_LOG_TRIVIAL(warning) << message; break;
            case LogLevel::Error: BOOST_LOG_TRIVIAL(error) << message; break;
            case LogLevel::Fatal: BOOST_LOG_TRIVIAL(fatal) << message; break;
        }
    }

private:
    Logger() = default;

    // 将 LogLevel 映射到 Boost.Log 的日志级别
    static logging::trivial::severity_level toBoostSeverity(LogLevel level) {
        switch (level) {
            case LogLevel::Trace: return logging::trivial::trace;
            case LogLevel::Debug: return logging::trivial::debug;
            case LogLevel::Info: return logging::trivial::info;
            case LogLevel::Warning: return logging::trivial::warning;
            case LogLevel::Error: return logging::trivial::error;
            case LogLevel::Fatal: return logging::trivial::fatal;
            default: return logging::trivial::info;
        }
    }

    // 设置日志级别过滤器
    static logging::filter getSeverityFilter(LogLevel level) {
        return logging::trivial::severity >= toBoostSeverity(level);
    }
};
