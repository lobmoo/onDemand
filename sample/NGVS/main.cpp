#include "log/logger.h"
#include "NgvsSerialize.h"
#include "KeyValueSerialize.h"
#include "ModelParser.h"
#include "iostream"
#include <fstream>
#include <stddef.h>

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




void test_NGVS()
{
    std::string xmlContent =
        readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/modelNgvs.xml");
    auto &parser = dsf::parser::ModelParser::getInstance();
    std::string scama;
    std::unordered_map<std::string, dsf::parser::ModelDefine> modelDefinels;
    parser.processModelSchema(xmlContent, modelDefinels, scama);

    // LOG(info) << "Model schema \n" << scama;

    // for(const auto &modelDefine : modelDefinels) {
    //     LOG(info) << "Model Name: " << modelDefine.second.modelName
    //               << ", schema: " << modelDefine.second.schema;
    // }

    auto modelDefines = parser.getModelDefines();
    for (const auto &modelDefine : modelDefines) {
        LOG(info) << "Model Name: " << modelDefine.first
                  << ", Version: " << modelDefine.second.modelVersion
                  << ", Size: " << modelDefine.second.size;
    }
    
    // // parser.printStructNode("InnerModel:1.0");
    // // parser.printHashCache();

    // dsf::ngvs::NgvsSerializer Serializer;
    // std::vector<char> outBuffer;
    // std::unordered_map<std::string, std::string> inData;
    // inData["STD.VAR1.B1"] = "123";
    // inData["STD.VAR1.B2"] = "true";
    // inData["STD.VAR1.B3.C1"] = "789";
    // inData["STD.VAR1.B3.C2[1]"] = "3.13";
    // inData["STD.VAR1.B3.C2[2]"] = "3.14";
    // inData["STD.VAR2[1]"] = "16";
    // inData["STD.VAR2[2]"] = "17";
    // inData["STD.VAR3"] = "18";
    // inData["STD.VAR4"] = "19";
    // inData["STD.VAR5"] = "20";
    // std::vector<std::shared_ptr<dsf::parser::TreeNode>> leaves;
    // dsf::parser::ModelDefine model;
    // if (!Serializer.serialize("STD.NGVS_S1:1.0", inData, outBuffer)) {
    //     LOG(error) << "Serialization failed";
    //     return;
    // }
    // std::unordered_map<std::string, std::string> outData;
    // if (!Serializer.deserialize("STD.NGVS_S1:1.0", outBuffer, outData)) {
    //     LOG(error) << "deserialize failed";
    //     return;
    // }
    // for (auto &pair : outData) {
    //     LOG(info) << "Key: " << pair.first << ", Value: " << pair.second;
    // }
}

int main(int argc, char *argv[])
{
    Logger::Instance()->Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    test_NGVS();
    return 0;
}