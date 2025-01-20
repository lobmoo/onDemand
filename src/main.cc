#include"logger.h"
#include <unistd.h>

int main()
{   
    Logger lg;
    lg.Init("log/test.log", 0, 0, 1, 100);
    while (1)
    {
        LOG_TRACE("test info");
        LOG_DEBUG("test info");
        LOG_INFO("test info");
        LOG_WARN("test info");
        LOG_ERROR("test info");
        LOG_FATAL("test info");
        usleep(1000);
        
    }
    
   

    return 0;
}
