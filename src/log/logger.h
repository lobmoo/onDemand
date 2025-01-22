#ifndef LOGGER_H
#define LOGGER_H

#include <stdexcept>
#include <string>
#include <iostream>
#include <fstream>

#include <boost/log/common.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>

class Logger
{
public:
    enum LoggerType
    {
        both = 0,
        console,
        file,

    };
    typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> file_sink;
    typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_file_backend> async_file_sink;
    typedef boost::log::v2s_mt_posix::sinks::synchronous_sink<boost::log::v2s_mt_posix::sinks::text_ostream_backend> console_sink;
    typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_ostream_backend> async_console_sink;

    ~Logger() {}
    static Logger &Instance();
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> _logger;
    bool Init(std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex);
    void setConsoleLogLevel(const std::string &level);
    void setFileLogLevel(const std::string& level);

private:
    Logger() {}
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    boost::shared_ptr<console_sink> consoleSink_ = nullptr;
    boost::shared_ptr<file_sink> fileSink_ = nullptr;

private:
    inline void setconsoleSink();
    inline void setfileSink(std::string fileName, int maxFileSize, int maxBackupIndex);
};

#define LOG_EXTRA_INFO                                                                 \
    boost::log::attribute_cast<boost::log::attributes::mutable_constant<std::string>>( \
        boost::log::core::get()->get_global_attributes()["File"])                      \
        .set(__FILE__);                                                                \
    boost::log::attribute_cast<boost::log::attributes::mutable_constant<int>>(         \
        boost::log::core::get()->get_global_attributes()["Line"])                      \
        .set(__LINE__);

#define LOG_TRACE         \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO;       \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::trace)

#define LOG_DEBUG         \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO;       \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::debug)

#define LOG_INFO          \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO;       \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::info)

#define LOG_WARN          \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO;       \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::warning)

#define LOG_ERROR         \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO;       \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::error)

#define LOG_FATAL         \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO;       \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::fatal)

#endif
