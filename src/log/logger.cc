#include "logger.h"
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>



#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

Logger& Logger::Instance() {
    static Logger log;
    return log;
} 

boost::log::formatter formatter =
        boost::log::expressions::stream
        << "["
        <<  boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S.%f")
        << "|"
        << boost::log::expressions::attr<
               boost::log::attributes::current_thread_id::value_type>("ThreadID")
        << "](" << boost::log::expressions::attr<std::string>("File") << ":"
        << boost::log::expressions::attr<int>("Line") << ") "
        << " [" << boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") << "]"
        << boost::log::expressions::smessage;


boost::log::formatter color_formatter =
        boost::log::expressions::stream
        << "[" 
        << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
        << "|" 
        << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
        << "](" 
        << boost::log::expressions::attr<std::string>("File") 
        << ":"
        << boost::log::expressions::attr<int>("Line") 
        << ") " 
        << boost::log::expressions::if_(boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::debug)
            [ boost::log::expressions::stream << BLUE ]
        << boost::log::expressions::if_(boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::info)
            [ boost::log::expressions::stream << GREEN ]
        << boost::log::expressions::if_(boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::warning)
            [ boost::log::expressions::stream << YELLOW ]
        << boost::log::expressions::if_(boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::error)
            [ boost::log::expressions::stream << RED ]
        << " [" << boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity") << "]" 
        << RESET
        << " " 
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

    boost::log::add_common_attributes();
    boost::log::core::get()->add_global_attribute("File", boost::log::attributes::mutable_constant<std::string>(""));
    boost::log::core::get()->add_global_attribute("Line", boost::log::attributes::mutable_constant<int>(0));
    return true;
}


