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

    ~Logger() {}
    static Logger &Instance();
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> _logger;
    bool Init(std::string fileName, int type, int level, int maxFileSize, int maxBackupIndex);

private:
    Logger() {}
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> file_sink;
};

#define LOG_EXTRA_INFO                                                                 \
    boost::log::attribute_cast<boost::log::attributes::mutable_constant<std::string>>( \
        boost::log::core::get()->get_global_attributes()["File"])                      \
        .set(__FILE__);                                                                \
    boost::log::attribute_cast<boost::log::attributes::mutable_constant<int>>(         \
        boost::log::core::get()->get_global_attributes()["Line"])                      \
        .set(__LINE__);


#define LOG_TRACE \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO; \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::trace)

#define LOG_DEBUG \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO; \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::debug)

#define LOG_INFO \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO; \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::info)

#define LOG_WARN \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO; \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::warning)

#define LOG_ERROR \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO; \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::error)

#define LOG_FATAL \
    BOOST_LOG_FUNCTION(); \
    LOG_EXTRA_INFO; \
    BOOST_LOG_SEV(Logger::Instance()._logger, boost::log::trivial::fatal)

#endif
