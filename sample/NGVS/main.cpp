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
    std::string xmlContent = readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/mode.xml");
    auto &Serializer = dsf::ngvs::NgvsSerializer::getInstance();
    std::vector<char> outBuffer;
    std::unordered_map<std::string, std::string> inData;
    inData["STD.VAR1.B1"]="3";
    inData["STD.VAR2.B2"] = "true";
    inData["STD.VAR3:B3.C1"] = "10";
    inData["STD.VAR3:B3.C2"] = "20";
    if (!Serializer.serialize(xmlContent, "InnerModel3:1.0", inData, outBuffer)) {
        LOG(error) << "Serialization failed";
        return 1;
    }
    // while (std::cin.get() != '\n') {
    // }

    return 0;
}