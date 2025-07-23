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
    std::string xmlContent = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model3.xml");
    auto &parser = dsf::parser::ModelParser::getInstance();
    std::string scama;
    std::unordered_map<std::string, dsf::parser::ModelDefine> modelDefinels;
    parser.processModelSchema(xmlContent, modelDefinels, scama);

    // parser.printHashCache();
    // LOG(info) << "++++++++++++++++++++++++++++++++ \n";

    // LOG(info) << "Model schema \n" << scama;

    for (const auto &modelDefine : modelDefinels) {
        LOG(info) << "Model Name: " << modelDefine.first
                  << ", Version: " << modelDefine.second.modelVersion
                  << ", Size: " << modelDefine.second.size;
    }
    LOG(info) << "++++++++++++++++++++++++++++++++ \n";
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

#pragma pack(push, 4)
struct InnerModel3 {
    int32_t long_array[1][2][1];
    int16_t short_sequence[5];
};

struct InnerModel2 {
    // int32_t third;
    // <member name="complex_member3" type="nonBasic" nonBasicTypeName="InnerModel3" version="1.0"/>
    InnerModel3 complex_member3;
};

struct InnerModel {
    int32_t first;
    InnerModel2 complex_member2;
};

struct ParentModel {
    float first;
    int64_t second;
};

struct ComplexModel : public ParentModel {
    // <member name="complex_member" type="nonBasic" nonBasicTypeName="InnerModel" version="1.0"/>
    InnerModel complex_member;
};

struct InnerModel3_local {
    int32_t long_array[2][2][1];
    int16_t short_sequence[6];
};

struct InnerModel2_local {
    int32_t third;
    // <member name="complex_member3" type="nonBasic" nonBasicTypeName="InnerModel3" version="1.0"/>
    InnerModel3_local complex_member3;
};

struct InnerModel_local {
    int32_t first;
    InnerModel2_local complex_member2;
};

struct ParentModel_local {
    float first;
    int64_t second;
};

struct ComplexModel_local : public ParentModel_local {
    // <member name="complex_member" type="nonBasic" nonBasicTypeName="InnerModel" version="1.0"/>
    InnerModel_local complex_member;
};

struct SimpleModel {
    bool DT_BOOLEAN;          // 布尔，1字节
    int8_t DT_BYTE;           // 8位，1字节
    int16_t DT_WORD;          // 16位，2字节
    int32_t DT_DWORD;         // 32位，4字节
    int64_t DT_LWORD;         // 64位，8字节
    int8_t DT_SINT;           // 8位有符号整数，1字节
    uint8_t DT_USINT;         // 8位无符号整数，1字节
    int16_t DT_INT;           // 16位有符号整数，2字节
    uint16_t DT_UINT;         // 16位无符号整数，2字节
    int32_t DT_DINT;          // 32位有符号整数，4字节
    uint32_t DT_UDINT;        // 32位无符号整数，4字节
    int64_t DT_LINT;          // 64位有符号整数，8字节
    uint64_t DT_ULINT;        // 64位无符号整数，8字节
    float DT_REAL;            // 32位浮点数，4字节
    double DT_LREAL;          // 64位浮点数，8字节
    char DT_CHAR;             // 单字节字符，1字节
    char DT_CHARSEQ[82];      // 单字节字符数组，默认N=80，占82字节
    char DT_STRING[82];       // 字符串，假设与DT_CHARSEQ相同
    wchar_t DT_WCHAR;         // 宽字节字符，2字节
    wchar_t DT_WCHARSEQ[254]; // 宽字节字符数组，默认N=254，占508字节
    wchar_t DT_WSTRING[254];  // 宽字符串，假设与DT_WCHARSEQ相同
};
#pragma pack(pop)

void test_struct1()
{
#if 1
    std::string xmlContent =
        readXmlFile("/home/wwk/workspaces/TEMP/test_demo/sample/NGVS/modelNgvs.xml");
    auto &modelParser = dsf::parser::ModelParser::getInstance();
    std::string processdSchema;
    std::unordered_map<std::string, dsf::parser::ModelDefine> modelDefines;
    bool ret = modelParser.processModelSchema(xmlContent, modelDefines, processdSchema);
    std::cout << "modelParser result: " << ret << std::endl;
    for (auto &model : modelDefines) {
        std::cout << "modelIndex: " << model.first << std::endl;
        std::cout << "modelName: " << model.second.modelName << std::endl;
        std::cout << "modelVersion: " << model.second.modelVersion << std::endl;
        std::cout << "modelSchema: " << model.second.schema << std::endl;
    }
    dsf::kvpair::KeyValueSerializer kvs = dsf::kvpair::KeyValueSerializer();
    std::unordered_map<std::string, std::string> inData = {
        {"first", "1"},
        {"second", "2"},
        {"complex_member.first", "3"},
        {"complex_member.complex_member2.complex_member3.long_array[0][0][0]", "4"},
        {"complex_member.complex_member2.complex_member3.long_array[0][1][0]", "5"},
        {"complex_member.complex_member2.complex_member3.short_sequence[0]", "6"},
        {"complex_member.complex_member2.complex_member3.short_sequence[1]", "7"},
        {"complex_member.complex_member2.complex_member3.short_sequence[2]", "8"},
        {"complex_member.complex_member2.complex_member3.short_sequence[3]", "9"},
        {"complex_member.complex_member2.complex_member3.short_sequence[4]", "10"}};
    std::vector<char> outBuffer;
    ret = kvs.serialize("ComplexModel:c2b0cf57c2a26a2a9a689517743d53fd", inData, outBuffer);
    std::cout << "Serialize result: " << ret << std::endl;
    std::unordered_map<std::string, std::string> outData;
    ret = kvs.deserialize("ComplexModel:c2b0cf57c2a26a2a9a689517743d53fd", outBuffer, outData);
    std::cout << "deserialize result: " << ret << std::endl;
    std::cout << "outBuffer size: " << outBuffer.size() << std::endl;
    for (auto &item : outData) {
        std::cout << item.first << ":" << item.second << std::endl;
    }
    ComplexModel *complexVar = reinterpret_cast<ComplexModel *>(outBuffer.data());
    std::cout << "short_sequence[4]: "
              << complexVar->complex_member.complex_member2.complex_member3.short_sequence[4]
              << std::endl;
    std::cout << "first: " << complexVar->first << std::endl;
    std::cout << "array010: "
              << complexVar->complex_member.complex_member2.complex_member3.long_array[0][1][0]
              << std::endl;

#endif
}

int main(int argc, char *argv[])
{
    Logger::Instance()->Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    // test_NGVS();
    test_struct1();
    return 0;
}