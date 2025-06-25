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
// #pragma pack(push, 4)  // 强制对齐为4字节
// struct TestNormalModel {
//     _Float32 first;   // 4 字节
//     int64_t second;   // 8 字节，允许非8字节对齐
//     int8_t third;     // 1 字节
// };
// #pragma pack(pop)
#pragma pack(push, 2)  // 强制对齐为2字节
struct InnerModel3 {
    int32_t long_array[1][2][1];
    int16_t short_sequence[5];
};

struct InnerModel2 {
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
#pragma pack(pop)

void test1()
{
    /* 1. 初始化日志和xml内容 */
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    std::string xmlContent0 = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model0.xml");
    std::string xmlContent1 = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model1.xml");
    std::string xmlContent2 = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model2.xml");
    auto serializer = dsf::kvpair::KeyValueSerializer();
    /* 2. 定义输入数据和输出空间 */
    // 基本元素
    float first = 1111111;  
    
    int64_t second = 2222222222222LL;
    // 嵌套一层
    int32_t complex_member2_third = 33333333;
    // 嵌套两层
    int32_t complex_member2_complex_member3_fourth = 44444444;
    // 嵌套两层+数组
    int32_t complex_member2_complex_member3_longarray0 = 55555555;
    int32_t complex_member2_complex_member3_longarray1 = 66666666;
    int32_t complex_member2_complex_member3_longarray2 = 77777777;
    int32_t complex_member2_complex_member3_longarray3 = 88888888;
    int32_t complex_member2_complex_member3_longarray4 = 99999999;
    int8_t fourth =  100;
    // 多个non-basic元素 & 不同层元素重名测试
    float complex_member3_complex_member6_first = 11.11f;
    int32_t complex_member3_complex_member6_second = 1212121212;
    int8_t complex_member3_complex_member6_third = 13;
    // 继承结构
    float complex_member4_first = 14.f;
    int64_t complex_member4_second = 151515151515LL;
    int32_t complex_member4_complex_member_fourth = 16161616;
    // 继承结构+数组
    int32_t complex_member4_complex_member_longarray0 = 17171717;
    int32_t complex_member4_complex_member_longarray1 = 18181818;
    int32_t complex_member4_complex_member_longarray2 = 19191919;
    int32_t complex_member4_complex_member_longarray3 = 20202020;
    int32_t complex_member4_complex_member_longarray4 = 21212121;
    // sequence
    int32_t short_sequence0 = 22222222;
    int32_t short_sequence1 = 23232323;
    int32_t short_sequence2 = 24242424;
    int32_t short_sequence3 = 25252525;
    int32_t short_sequence4 = 26262626;
    // struct array
    float struct_array0_first = 27.27f;
    int32_t struct_array0_second = 28282828;
    int8_t struct_array0_third = 29;
    float struct_array1_first = 30.30f;
    int32_t struct_array1_second = 31313131;
    int8_t struct_array1_third = 32;
    float struct_array2_first = 33.33f;
    int32_t struct_array2_second = 34343434;
    int8_t struct_array2_third = 35;
    float struct_array3_first = 36.36f;
    int32_t struct_array3_second = 37373737;
    int8_t struct_array3_third = 38;
    float struct_array4_first = 39.39f;
    int32_t struct_array4_second = 40404040;
    int8_t struct_array4_third = 41;
    std::unordered_map<std::string, std::string> data = {
        {"first", std::to_string(first)},
        {"second", std::to_string(second)},
        {"complex_member2.third", std::to_string(complex_member2_third)},
        {"complex_member2.complex_member3.fourth", std::to_string(complex_member2_complex_member3_fourth)},
        {"complex_member2.complex_member3.long_array[0]", std::to_string(complex_member2_complex_member3_longarray0)},
        {"complex_member2.complex_member3.long_array[1]", std::to_string(complex_member2_complex_member3_longarray1)},
        {"complex_member2.complex_member3.long_array[2]", std::to_string(complex_member2_complex_member3_longarray2)},
        {"complex_member2.complex_member3.long_array[3]", std::to_string(complex_member2_complex_member3_longarray3)},
        {"complex_member2.complex_member3.long_array[4]", std::to_string(complex_member2_complex_member3_longarray4)},
        {"fourth", std::to_string(fourth)},
        {"complex_member3.complex_member6.first", std::to_string(complex_member3_complex_member6_first)},
        {"complex_member3.complex_member6.second", std::to_string(complex_member3_complex_member6_second)},
        {"complex_member3.complex_member6.third", std::to_string(complex_member3_complex_member6_third)},
        {"complex_member4.first", std::to_string(complex_member4_first)},
        {"complex_member4.second", std::to_string(complex_member4_second)},
        {"complex_member4.complex_member.fourth", std::to_string(complex_member4_complex_member_fourth)},
        {"complex_member4.complex_member.long_array[0]", std::to_string(complex_member4_complex_member_longarray0)},
        {"complex_member4.complex_member.long_array[1]", std::to_string(complex_member4_complex_member_longarray1)},
        {"complex_member4.complex_member.long_array[2]", std::to_string(complex_member4_complex_member_longarray2)},
        {"complex_member4.complex_member.long_array[3]", std::to_string(complex_member4_complex_member_longarray3)},
        {"complex_member4.complex_member.long_array[4]", std::to_string(complex_member4_complex_member_longarray4)},
        {"short_sequence[0]", std::to_string(short_sequence0)},
        {"short_sequence[1]", std::to_string(short_sequence1)},
        {"short_sequence[2]", std::to_string(short_sequence2)},
        {"short_sequence[3]", std::to_string(short_sequence3)},
        {"short_sequence[4]", std::to_string(short_sequence4)},
        {"struct_array[0].first", std::to_string(struct_array0_first)},
        {"struct_array[0].second", std::to_string(struct_array0_second)},
        {"struct_array[0].third", std::to_string(struct_array0_third)},
        {"struct_array[1].first", std::to_string(struct_array1_first)},
        {"struct_array[1].second", std::to_string(struct_array1_second)},
        {"struct_array[1].third", std::to_string(struct_array1_third)},
        {"struct_array[2].first", std::to_string(struct_array2_first)},
        {"struct_array[2].second", std::to_string(struct_array2_second)},
        {"struct_array[2].third", std::to_string(struct_array2_third)},
        {"struct_array[3].first", std::to_string(struct_array3_first)},
        {"struct_array[3].second", std::to_string(struct_array3_second)},
        {"struct_array[3].third", std::to_string(struct_array3_third)},
        {"struct_array[4].first", std::to_string(struct_array4_first)},
        {"struct_array[4].second", std::to_string(struct_array4_second)},
        {"struct_array[4].third", std::to_string(struct_array4_third)},
    };
    std::vector<char> outBuff;

    /* 2.5解析model */
    auto &parser = dsf::parser::ModelParser::getInstance();
    std::string ret;
    parser.init(xmlContent0, ret);
    parser.init(xmlContent1, ret);
    parser.init(xmlContent2, ret);
    /* 3.序列化 */
    serializer.serialize("InnerModel44:1.0", data, outBuff);

    
    /* 4.输出序列化结果 */
    // TestNormalModel* model = (TestNormalModel*)(outBuff.data());
    // LOG(info) << "first: " << model->first << " second: " << model->second << " third: " << (int)(model->third);


    
    std::unordered_map<std::string, std::string> outData;
    serializer.deserialize("InnerModel44:1.0", outBuff, outData);

    LOG(info) << "first: " << outData["first"];
    LOG(info) << "second: " << outData["second"];
    LOG(info) << "complex_member2.third: " << outData["complex_member2.third"];
    LOG(info) << "complex_member2.complex_member3.fourth: " << outData["complex_member2.complex_member3.fourth"];
    LOG(info) << "complex_member2.complex_member3.long_array[0]: " << outData["complex_member2.complex_member3.long_array[0]"];
    LOG(info) << "complex_member2.complex_member3.long_array[1]: " << outData["complex_member2.complex_member3.long_array[1]"];
    LOG(info) << "complex_member2.complex_member3.long_array[2]: " << outData["complex_member2.complex_member3.long_array[2]"];
    LOG(info) << "complex_member2.complex_member3.long_array[3]: " << outData["complex_member2.complex_member3.long_array[3]"];
    LOG(info) << "complex_member2.complex_member3.long_array[4]: " << outData["complex_member2.complex_member3.long_array[4]"];
    LOG(info) << "fourth: " << outData["fourth"];
    LOG(info) << "complex_member3.complex_member6.first: " << outData["complex_member3.complex_member6.first"];
    LOG(info) << "complex_member3.complex_member6.second: " << outData["complex_member3.complex_member6.second"];
    LOG(info) << "complex_member3.complex_member6.third: " << outData["complex_member3.complex_member6.third"];
    LOG(info) << "complex_member4.first: " << outData["complex_member4.first"];
    LOG(info) << "complex_member4.second: " << outData["complex_member4.second"];
    LOG(info) << "complex_member4.complex_member.fourth: " << outData["complex_member4.complex_member.fourth"];
    LOG(info) << "complex_member4.complex_member.long_array[0]: " << outData["complex_member4.complex_member.long_array[0]"];
    LOG(info) << "complex_member4.complex_member.long_array[1]: " << outData["complex_member4.complex_member.long_array[1]"];
    LOG(info) << "complex_member4.complex_member.long_array[2]: " << outData["complex_member4.complex_member.long_array[2]"];
    LOG(info) << "complex_member4.complex_member.long_array[3]: " << outData["complex_member4.complex_member.long_array[3]"];
    LOG(info) << "complex_member4.complex_member.long_array[4]: " << outData["complex_member4.complex_member.long_array[4]"];
    LOG(info) << "short_sequence[0]: " << outData["short_sequence[0]"];
    LOG(info) << "short_sequence[1]: " << outData["short_sequence[1]"];
    LOG(info) << "short_sequence[2]: " << outData["short_sequence[2]"];
    LOG(info) << "short_sequence[3]: " << outData["short_sequence[3]"];
    LOG(info) << "short_sequence[4]: " << outData["short_sequence[4]"];
    LOG(info) << "struct_array[0].first: " << outData["struct_array[0].first"];
    LOG(info) << "struct_array[0].second: " << outData["struct_array[0].second"];
    LOG(info) << "struct_array[0].third: " << outData["struct_array[0].third"];
    LOG(info) << "struct_array[1].first: " << outData["struct_array[1].first"];
    LOG(info) << "struct_array[1].second: " << outData["struct_array[1].second"];
    LOG(info) << "struct_array[1].third: " << outData["struct_array[1].third"];
    LOG(info) << "struct_array[2].first: " << outData["struct_array[2].first"];
    LOG(info) << "struct_array[2].second: " << outData["struct_array[2].second"];
    LOG(info) << "struct_array[2].third: " << outData["struct_array[2].third"];
    LOG(info) << "struct_array[3].first: " << outData["struct_array[3].first"];
    LOG(info) << "struct_array[3].second: " << outData["struct_array[3].second"];
    LOG(info) << "struct_array[3].third: " << outData["struct_array[3].third"];
    LOG(info) << "struct_array[4].first: " << outData["struct_array[4].first"];
    LOG(info) << "struct_array[4].second: " << outData["struct_array[4].second"];
    LOG(info) << "struct_array[4].third: " << outData["struct_array[4].third"];


    // for(auto &pair : outData) {
    //     LOG(info) << "Key: " << pair.first << ", Value: " << pair.second;
    // }

    while (std::cin.get() != '\n') {
    }
}
// void test2()
// {
//         /* 1. 初始化日志和xml内容 */
//     Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
//     std::string xmlContent = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/test.xml");
//     auto &serializer = dsf::kvpair::KeyValueSerializer::getInstance();

//     /* 2. 定义输入数据和输出空间 */
//     // 基本元素
//     float first = 1111.11;
//     int64_t second = 2222222222222LL;
//     int32_t complex_member_first = 333333;
//     int32_t complex_member_complex_member2_complex_member3_longarray000 = 444444;
//     int32_t complex_member_complex_member2_complex_member3_longarray010 = 555555;
//     int16_t complex_member_omplex_member2_complex_member3_short_sequence0 = 666;
//     int16_t complex_member_complex_member2_complex_member3_short_sequence1 = 777;
//     int16_t complex_member_complex_member2_complex_member3_short_sequence2 = 888;
//     int16_t complex_member_complex_member2_complex_member3_short_sequence3 = 999;
//     int16_t complex_member_complex_member2_complex_member3_short_sequence4 = 1010;
//     std::unordered_map<std::string, std::string> data = {
//         {"first", std::to_string(first)},
//         {"second", std::to_string(second)},
//         {"complex_member.first", std::to_string(complex_member_first)},
//         {"complex_member.complex_member2.complex_member3.long_array[0][0][0]", std::to_string(complex_member_complex_member2_complex_member3_longarray000)},
//         {"complex_member.complex_member2.complex_member3.long_array[0][1][0]", std::to_string(complex_member_complex_member2_complex_member3_longarray010)},
//         {"complex_member.complex_member2.complex_member3.short_sequence[0]", std::to_string(complex_member_omplex_member2_complex_member3_short_sequence0)},
//         {"complex_member.complex_member2.complex_member3.short_sequence[1]", std::to_string(complex_member_complex_member2_complex_member3_short_sequence1)},
//         {"complex_member.complex_member2.complex_member3.short_sequence[2]", std::to_string(complex_member_complex_member2_complex_member3_short_sequence2)},
//         {"complex_member.complex_member2.complex_member3.short_sequence[3]", std::to_string(complex_member_complex_member2_complex_member3_short_sequence3)},
//         {"complex_member.complex_member2.complex_member3.short_sequence[4]", std::to_string(complex_member_complex_member2_complex_member3_short_sequence4)}
//     };
//     std::vector<char> outBuff;

//     /* 3.序列化 */
//     serializer.serialize(xmlContent, "ComplexModel:3.0", data, outBuff);

    
//     /* 4.输出序列化结果 */
//     ComplexModel* model = (ComplexModel*)(outBuff.data());
//     LOG(info) << "first: " << model->first;
//     LOG(info) << "second: " << model->second;
//     LOG(info) << "complex_member.first: " << model->complex_member.first;
//     LOG(info) << "complex_member2.complex_member3.long_array[0][0][0]: " << model->complex_member.complex_member2.complex_member3.long_array[0][0][0];
//     LOG(info) << "complex_member2.complex_member3.long_array[0][1][0]: " << model->complex_member.complex_member2.complex_member3.long_array[0][1][0];
//     LOG(info) << "complex_member2.complex_member3.short_sequence[0]: " << model->complex_member.complex_member2.complex_member3.short_sequence[0];
//     LOG(info) << "complex_member2.complex_member3.short_sequence[1]: " << model->complex_member.complex_member2.complex_member3.short_sequence[1];
//     LOG(info) << "complex_member2.complex_member3.short_sequence[2]: " << model->complex_member.complex_member2.complex_member3.short_sequence[2];
//     LOG(info) << "complex_member2.complex_member3.short_sequence[3]: " << model->complex_member.complex_member2.complex_member3.short_sequence[3];
//     LOG(info) << "complex_member2.complex_member3.short_sequence[4]: " << model->complex_member.complex_member2.complex_member3.short_sequence[4];




//     /* 5.反序列化 */
//     // std::unordered_map<std::string, std::string> outData;
//     // serializer.deserialize(xmlContent, "InnerModel:1.0", outBuff, outData);

//     // LOG(info) << "first: " << outData["first"];
//     // LOG(info) << "complex_member2.complex_member3.long_array[0][0][0]: " << outData["complex_member2.complex_member3.long_array[0][0][0]"];
//     // LOG(info) << "complex_member2.complex_member3.long_array[0][1][0]: " << outData["complex_member2.complex_member3.long_array[0][1][0]"];
//     // LOG(info) << "complex_member2.complex_member3.short_sequence[0]: " << outData["complex_member2.complex_member3.short_sequence[0]"];
//     // LOG(info) << "complex_member2.complex_member3.short_sequence[1]: " << outData["complex_member2.complex_member3.short_sequence[1]"];
//     // LOG(info) << "complex_member2.complex_member3.short_sequence[2]: " << outData["complex_member2.complex_member3.short_sequence[2]"];
//     // LOG(info) << "complex_member2.complex_member3.short_sequence[3]: " << outData["complex_member2.complex_member3.short_sequence[3]"];
//     // LOG(info) << "complex_member2.complex_member3.short_sequence[4]: " << outData["complex_member2.complex_member3.short_sequence[4]"];



//     while (std::cin.get() != '\n') {
//     }
// }
// void test3()
// {
//     /* 1. 初始化日志和xml内容 */
//     Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);

//     ComplexModel var;
//     LOG(error) << "Model Size: " << sizeof(ComplexModel);
//     var.complex_member.complex_member2.complex_member3.short_sequence[4] = 123;
//     std::unordered_map<std::string, std::string> pairs;
//     dsf::kvpair::KeyValueSerializer keyValueSerializer =
//         dsf::kvpair::KeyValueSerializer::getInstance();
//     std::string modelIndex = "ComplexModel:3.0";
//     std::string schema = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/test.xml");
//     std::cout << schema << std::endl;
//     std::cout << "keyValuePair.size(): " << pairs.size() << std::endl;
//     char *byteData = static_cast<char *>((void *)&var);
//     std::vector<char> vec(byteData, byteData + sizeof(ComplexModel));

//     keyValueSerializer.deserialize(schema, modelIndex, vec, pairs);
//     std::cout << pairs.size();
//     for (auto pair : pairs) {
//         std::cout << pair.first << ":" << pair.second << std::endl;
//     }

//     while (std::cin.get() != '\n') {
//     }
// }



int testNgvs()
{
    std::string xmlContent =
        readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/modelNgvs.xml");
    auto &parser = dsf::parser::ModelParser::getInstance();  
    std::string error_text;
    parser.init(xmlContent, error_text);
    dsf::ngvs::NgvsSerializer Serializer;
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

    std::vector<std::shared_ptr<dsf::parser::TreeNode>> leaves;
    dsf::parser::ModelDefine model;

  
    if (!Serializer.serialize("STD.NGVS_S1:1.0", inData, outBuffer)) {
        LOG(error) << "Serialization failed";
        return 1;
    }

    std::unordered_map<std::string, std::string> outData;
    if (!Serializer.deserialize("STD.NGVS_S1:1.0", outBuffer, outData)) {
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


int main(int argc, char *argv[])
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    testNgvs();

    return 0;
}