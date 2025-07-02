#include <log/logger.h>
#include <string>


struct stu
{
    uint8_t a;
    uint8_t b;
   
};


int main(int argc, char *argv[])
{
    Logger::Instance().Init("");
  
    LOG(info) << "stu Size: " << sizeof(stu);
    return 0;
}