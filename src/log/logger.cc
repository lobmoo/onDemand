#include "logger.h"
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>

Logger& Logger::Instance() {
    static Logger log;
    return log;
} boost::log::formatter formatter =
        boost::log::expressions::stream
        << "["
        << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
               "TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
        << "|"
        << boost::log::expressions::attr<
               boost::log::attributes::current_thread_id::value_type>("ThreadID")
        << "]"
        << boost::log::expressions::format_named_scope(
               "Scope", boost::log::keywords::format = "(%f:%l)",
               boost::log::keywords::iteration = boost::log::expressions::reverse,
               boost::log::keywords::depth = 1)
        << "  ["
        << boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity")
        << "]"
        << boost::log::expressions::smessage;

bool Logger::Init(std::string fileName, int type, int level, int maxFileSize,
                  int maxBackupIndex) {
    
    switch (type) {
        case both: {
            auto consoleSink = boost::log::add_console_log();
            consoleSink->set_formatter(formatter);
            boost::log::core::get()->add_sink(consoleSink);
        } break;
        case console: {
            auto consoleSink = boost::log::add_console_log();
            consoleSink->set_formatter(formatter);
            boost::log::core::get()->add_sink(consoleSink);
        } break;
        case file: {
           
        } break;
        default: {
           
        } break;
    }
    boost::log::add_common_attributes();
    boost::log::core::get()->add_global_attribute(
        "Scope", boost::log::attributes::named_scope());
    boost::log::core::get()->set_filter(boost::log::trivial::severity >= level);
    return true;
}

