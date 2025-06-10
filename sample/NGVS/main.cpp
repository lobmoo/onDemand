#include "log/logger.h"
#include "NgvsSerialize.h"
#include "KeyValueSerialize.h"
#include "ModelParser.h"
#include "iostream"
#include <fstream>

std::string readXmlFile(const std::string &filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG(error) << "err: open file fail " << filePath;
        return "";
    }
    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }
    return content;
}

int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    // std::string xmlContent = readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/model.xml");
    // dsf::ngvs::NgvsSerializer serializer;
    // std::vector<char> outBuffer;
    // if (!serializer.serialize(xmlContent, "InnerModel:1.0", outBuffer)) {
    //     LOG(error) << "Serialization failed";
    //     return 1;
    // }
    std::string xmlContent = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model.xml");
    dsf::ngvs::KeyValueSerializer serializer;
    std::unordered_map<std::string, char *> data = {
        {"key1", (char *)"value1"},
        {"key2", (char *)"value2"},
        {"key3", (char *)"value3"}
    };
    serializer.serialize(xmlContent, "InnerModel:1.0", data);
    while (std::cin.get() != '\n') {
    }

    return 0;
}