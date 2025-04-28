#include <log/logger.h>



int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    LOG(info) << "hello world";
}