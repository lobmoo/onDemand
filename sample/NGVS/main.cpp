#include "log/logger.h"
#include "NgvsSerialize.h"
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
    Logger::Instance().Init(""); 
    std::string xmlContent = readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/model.xml");
    dsf::ngvs::ModelParser *parser = new dsf::ngvs::ModelParser();
    std::map<std::string, dsf::ngvs::ModelDefine> modelDefines;
    std::string error_message;
    dsf::ngvs::error_code_t ret = parser->parseSchema(modelDefines, xmlContent, error_message);
    if (ret != dsf::ngvs::MODEL_PARSER_OK)
        LOG(error) << "error_message: " << error_message;
    for (auto &entry : modelDefines) {
        LOG(info) << "++++++++++++++++++++++++++++++++++++++++";
        std::string modelName = entry.first;
        dsf::ngvs::ModelDefine modelDefine = entry.second;
        LOG(info) << "Model Name: " << modelName << ", Model Version: " << modelDefine.modelVersion
                  << ", Size: " << modelDefine.size << "\n";
        // for (auto &entry2 : modelDefine.mapKeyType) {
        //     LOG(warning) << "key: " << entry2.first << ", value: " << entry2.second;
        // }
    }
    while (std::cin.get() != '\n') {
    }
    return 0;
}