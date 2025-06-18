#include <log/logger.h>
#include "fastdds_wrapper/DDSQosConfigXmlFilter.h"
#include <string>


using namespace tinyxml2;


int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);
    DDSQosConfigXmlFilter filter;
    std::string xml = filter.filterQoS("/home/wwk/workspaces/test_demo/core/qosConfig.xml");
    LOG(info) << "Filtered XML: " << xml;
    return 0;
}