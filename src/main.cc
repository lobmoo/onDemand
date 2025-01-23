#include <iostream>
#include <unistd.h>
#include "logger.h"

int main() {
  // 初始化日志记录器
  if (!Logger::Instance().Init("myapp.log", Logger::both, 0, 5, 3, false)) {
    std::cerr << "Failed to initialize logger" << std::endl;
    return 1;
  }

  // 设置控制台和文件的日志级别
//   Logger::Instance().setConsoleLogLevel("info");
//   Logger::Instance().setFileLogLevel("info");

  while (1)
  {
    LOG_TRACE("This is a trace message");
    LOG_DEBUG( "This is a debug message");
    LOG_INFO ("This is an info message");
    LOG_WARNING ("This is a warning message");
    LOG_ERROR( "This is an error message");
    LOG_FATAL( "This is a fatal message");
    // LOG_DEBUG << "This is a debug message";
    //  LOG_INFO << "This is an info message";
    //  LOG_WARN << "This is a warning message";
    //  LOG_ERROR << "This is an error message";
    //  LOG_FATAL << "This is a fatal message";
    usleep(1000);
  }
  
  
}

