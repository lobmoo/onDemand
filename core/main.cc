#include <log/logger.h>
#include <string>





int main(int argc, char *argv[])
{
    Logger::Instance().Init("");
  
    LOG(info) << "Filtered XML:   111";
    return 0;
}