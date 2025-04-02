#include <unistd.h>

#include <iostream>
#include <thread>
#include <vector>

#include "log/logger.h"
#include "request_reply_api/DDSRequestReplyClient.h"
#include "request_reply_api/DDSRequestReplyServer.h"

#include "RequestReplyTypes/Calculator.hpp"
#include "RequestReplyTypes/CalculatorPubSubTypes.hpp"

#include "config/xmlConfig.h" 

void reply_callback(const CalculatorReplyType& reply, const SampleInfo& info) {
  LOG(info) << "+++++++++++++++++++++++Custom callback: Reply received with result '" << reply.result() << "'";
};


void request_callback(const CalculatorRequestType& request, CalculatorReplyType& reply) {
  int result = 0;
  result = request.x() + request.y();
  reply.client_id("x");
  reply.result(result);
  LOG(info) << "+++++++++++++++++++++++request_callback";
};


int main(int argc, char* argv[]) {
  Logger::Instance().setFlushOnLevel(Logger::trace);
  Logger::Instance().Init("log/myapp.log", Logger::both, Logger::trace, 60, 5);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " svr/cli" << std::endl;
    return -1;
  }
  if (strcmp(argv[1], "svr") == 0) {
    auto ptr = std::make_shared<request_reply::DDSRequestReplyServer<
        CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType, CalculatorReplyType>>(
        SERVER_NAME, request_callback);
  } else if (strcmp(argv[1], "cli") == 0) {
    auto ptr2 = std::make_shared<request_reply::DDSRequestReplyClient<
        CalculatorRequestTypePubSubType, CalculatorReplyTypePubSubType, CalculatorRequestType, CalculatorReplyType>>(
        SERVER_NAME);
    CalculatorReplyType Reply;  
    CalculatorRequestType request;
    request.client_id();
    request.x(100);
    request.y(100);
    request.operation(CalculatorOperationType::ADDITION);
    if (ptr2->send_request_for_wait(request, Reply, 3000)) {
      std::cout << "send request success" << std::endl;
      LOG(info) << "Reply received with result '" << Reply.result() << "'";
    }
  } else {
    std::cerr << "unknown command: " << argv[1] << std::endl;
  }
  while (std::cin.get() != '\n') {
  }
  return 0;
}


