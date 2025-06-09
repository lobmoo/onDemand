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
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    std::string xmlContent = readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/model.xml");
    // dsf::ngvs::ModelParser *parser = new dsf::ngvs::ModelParser();
    // std::map<std::string, dsf::ngvs::ModelDefine> modelDefines;
    // std::string error_message;
    // dsf::ngvs::error_code_t ret = parser->parseSchema(modelDefines, xmlContent, error_message);
    // if (ret != dsf::ngvs::MODEL_PARSER_OK)
    //     LOG(error) << "error_message: " << error_message;
    // for(auto model:modelDefines)
    // {
    //     LOG(info) << "model name: " << model.second.modelName << "  model name: " << model.second.modelVersion << "  model size: " << model.second.size;
    //     for(auto &ele:model.second.mapKeyType)
    //     {
    //         LOG(info) << "ele name: " << ele.first << "  ele type: " << ele.second.type << "  ele size: " << ele.second.size << "  ele offset: " << ele.second.offset;
    //     }
    //     LOG(info) << "----------------------------/n";
    // }

    dsf::ngvs::ModelParser parser;
    std::string error_message;

    std::map<std::string, dsf::ngvs::ModelDefine> modelDefines_;
    /*1.解析xml数据*/
    dsf::ngvs::error_code_t ret = parser.parseSchema(modelDefines_, xmlContent, error_message);
    if (ret != dsf::ngvs::MODEL_PARSER_OK) {
        LOG(error) << "parse schema failed, ret: " << ret;
        return false;
    }

    std::vector<dsf::ngvs::TreeNode> leaves;
    dsf::ngvs::ModelDefine model = modelDefines_["InnerModel:1.0"];
    if (model.members.empty()) {
        LOG(error) << "Model members are empty for model: InnerModel:1.0";
        return 1;
    }
    LOG(info) << "Model name: " << model.modelName << ", version: " << model.modelVersion
              << ", size: " << model.size;

    // if (parser.findNodeAndGetLeaves(model, "complex_member2:1.0", leaves)) {
    //     for (const auto &leaf : leaves) {
    //         LOG(info) << "Leaf: name=" << leaf.name << ", type=" << leaf.type
    //                   << ", size=" << leaf.size << ", offset=" << leaf.offset;
    //     }
    // }

    // parser.printInfo(model.members);
    parser.printLeafNodes(model);
    while (std::cin.get() != '\n') {
    }

    return 0;
}