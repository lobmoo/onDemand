#include <unistd.h>

#include <iostream>
#include <thread>
#include <vector>
#include "request_reply_api/DDSRequestReplyServer.h"
#include "request_reply_api/DDSRequestReplyClient.h"
#include "log/logger.h"




int main(int argc, char *argv[]) {
  Logger::Instance().setFlushOnLevel(Logger::trace);
  Logger::Instance().Init("log/myapp.log", Logger::both, Logger::trace, 60, 5);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " svr/cli" << std::endl;
    return -1;
  }
  if (strcmp(argv[1], "svr") == 0) {
    auto ptr = std::make_shared<request_reply::DDSRequestReplyServer>();  
  } else if (strcmp(argv[1], "cli") == 0) {
    auto ptr2 = std::make_shared<request_reply::DDSRequestReplyClient>();
    ptr2->run();
    
  } else {
    std::cerr << "unknown command: " << argv[1] << std::endl;
  }
  while (std::cin.get() != '\n') {
  }
 
  return 0;
}
