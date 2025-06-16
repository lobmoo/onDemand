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
    std::string xmlContent =
        readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/modelNgvs.xml");
    auto &Serializer = dsf::ngvs::NgvsSerializer::getInstance();
    std::vector<char> outBuffer;
    std::unordered_map<std::string, std::string> inData;
    inData["STD.VAR1.B1"] = "123";
    inData["STD.VAR1.B2"] = "true";
    inData["STD.VAR1.B3.C1"] = "789";
    inData["STD.VAR1.B3.C2[0]"] = "3.13";
    inData["STD.VAR1.B3.C2[1]"] = "3.14";
    inData["STD.VAR2[0]"] = "16";
    inData["STD.VAR2[1]"] = "17";
    inData["STD.VAR3"] = "18";
    inData["STD.VAR4"] = "19";
    inData["STD.VAR5"] = "20";


    if (!Serializer.serialize(xmlContent, "STD.NGVS_S1:1.0", inData, outBuffer)) {
        LOG(error) << "Serialization failed";
        return 1;
    }

    std::unordered_map<std::string, std::string> outData;
    if (!Serializer.deserialize(xmlContent, "STD.NGVS_S1:1.0", outBuffer, outData)) {
        LOG(error) << "deserialize failed";
        return 1;
    }

    for(auto &pair : outData) {
        LOG(info) << "Key: " << pair.first << ", Value: " << pair.second;
    }

    while (std::cin.get() != '\n') {
    }

    return 0;
}