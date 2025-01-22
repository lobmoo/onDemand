#include "logger.h"
#include <unistd.h>

int main()
{
    auto &prt = Logger::Instance();
    prt.Init("log/app.log", 0, 0, 1, 1);
    int a = 100;
    prt.setConsoleLogLevel("fatal");
    prt.setFileLogLevel("warning");
    while (1)
    {
        LOG_TRACE << "This is a trace message";
        LOG_DEBUG << "This is a debug message";
        LOG_INFO << "This is an info message";
        LOG_WARN << "This is a warning message";
        LOG_ERROR << "This is an error message";
        LOG_FATAL << "This is a fatal message";
        usleep(1000);
    }

    return 0;
}
