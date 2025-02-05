#include <unistd.h>

#include <iostream>

#include "logger.h"

int main() {
  // 初始化日志记录器
  if (!Logger::Instance().Init("myapp.log", Logger::both, 0, 5, 3, false)) {
    std::cerr << "Failed to initialize logger" << std::endl;
    return 1;
  }

  // 设置控制台和文件的日志级别
  Logger::Instance().setConsoleLogLevel("info");
  //   Logger::Instance().setFileLogLevel("info");

  while (1) {
    // LOG_DEBUG << "This is a debug message" << 42;
    // LOG_INFO << "This is an info message" << 12.5;
    // LOG_WARNING << "This is a warning message" << 'c';
    // LOG_ERROR << "This is an error message";
    // LOG_FATAL << "This is a fatal message";

    LOG(info) << "This is an info message with value: " << 42;
    LOG(error) << "Error occurred in module " <<  "  wwk";
    LOG(debug) << "jajasjsjjsja";
    LOG(trace) << "This is a trace message";
    LOG(fatal) << "This is a fatal message";

    usleep(1000);
  }
}
