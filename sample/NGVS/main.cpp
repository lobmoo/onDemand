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

#pragma pack(push, 2)
// __attribute__((packed, aligned(2)))
struct SimpleModel {
    bool DT_BOOLEAN;          // 布尔，1字节
    uint8_t  _pad0;
    int8_t DT_BYTE;           // 8位，1字节
    uint8_t  _pad1; 
    int16_t DT_WORD;          // 16位，2字节
    int32_t DT_DWORD;         // 32位，4字节
    int64_t DT_LWORD;         // 64位，8字节
    int8_t DT_SINT;           // 8位有符号整数，1字节
    uint8_t  _pad2;
    uint8_t DT_USINT;         // 8位无符号整数，1字节
    uint8_t  _pad3;
    int16_t DT_INT;           // 16位有符号整数，2字节
    uint16_t DT_UINT;         // 16位无符号整数，2字节
    int32_t DT_DINT;          // 32位有符号整数，4字节
    uint32_t DT_UDINT;        // 32位无符号整数，4字节
    int64_t DT_LINT;          // 64位有符号整数，8字节
    uint64_t DT_ULINT;        // 64位无符号整数，8字节
    float DT_REAL;            // 32位浮点数，4字节
    double DT_LREAL;          // 64位浮点数，8字节
    char DT_CHAR;             // 单字节字符，1字节
    uint8_t  _pad4;
    char DT_CHARSEQ[82];      // 单字节字符数组，默认N=80，占82字节
    char DT_STRING[82];       // 字符串，假设与DT_CHARSEQ相同
    // wchar_t DT_WCHAR;         // 宽字节字符，2字节
    // wchar_t DT_WCHARSEQ[254]; // 宽字节字符数组，默认N=254，占508字节
    // wchar_t DT_WSTRING[254];  // 宽字符串，假设与DT_WCHARSEQ相同
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
void test2()
{
    /* 1. 初始化日志和xml内容 */
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    std::string xmlContent0 = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model0.xml");
    std::string xmlContent1 = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model1.xml");
    std::string xmlContent2 = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model2.xml");
    auto serializer = dsf::kvpair::KeyValueSerializer();
    /* 2. 定义输入数据和输出空间 */
    // NormalTime
    int32_t NormalTime_first = 111111;
    uint32_t NormalTime_second = 222222;
    uint32_t NormalTime_third = 333333;
    uint32_t NormalTime_fourth = 444444;
    // LongTime
    int64_t LongTime_first = 5555555555;
    uint64_t LongTime_second = 6666666666;
    uint64_t LongTime_third = 7777777777;
    uint64_t LongTime_fourth = 8888888888;
    // String
    std::string String_first = "aaaaaaaaaaaaaaaaaaa";
    std::string String_second = "bbbbbbbbbbbbbbbbbbb";
    // char16_t String_third = u'我';
    // std::u16string String_fourth = u"你好世界";    
    // std::u16string String_fifth = u"你好世界2";

    // utf-16转utf-8
    std::unordered_map<std::string, std::string> data = {
        {"NormalTime.first", std::to_string(NormalTime_first)},
        {"NormalTime.second", std::to_string(NormalTime_second)},
        {"NormalTime.third", std::to_string(NormalTime_third)},
        {"NormalTime.fourth", std::to_string(NormalTime_fourth)},
        {"LongTime.first", std::to_string(LongTime_first)},
        {"LongTime.second", std::to_string(LongTime_second)},
        {"LongTime.third", std::to_string(LongTime_third)},
        {"LongTime.fourth", std::to_string(LongTime_fourth)},
        {"String.first", String_first},
        {"String.second", String_second}
    };
    std::vector<char> outBuff;

    /* 2.5解析model */
    auto &parser = dsf::parser::ModelParser::getInstance();
    std::string ret;
    parser.init(xmlContent0, ret);
    parser.init(xmlContent1, ret);
    parser.init(xmlContent2, ret);
    /* 3.序列化 */
    serializer.serialize("TestModel:1.0", data, outBuff);

    
    /* 4.输出序列化结果 */
    // TestNormalModel* model = (TestNormalModel*)(outBuff.data());
    // LOG(info) << "first: " << model->first << " second: " << model->second << " third: " << (int)(model->third);


    /* 5.反序列化 */
    std::unordered_map<std::string, std::string> outData;
    serializer.deserialize("TestModel2:1.0", outBuff, outData);

    LOG(info) << "NormalTime.first: " << outData["NormalTime.first"];
    LOG(info) << "NormalTime.second: " << outData["NormalTime.second"];
    LOG(info) << "NormalTime.third: " << outData["NormalTime.third"];
    LOG(info) << "NormalTime.fourth: " << outData["NormalTime.fourth"];
    LOG(info) << "LongTime.first: " << outData["LongTime.first"];
    LOG(info) << "LongTime.second: " << outData["LongTime.second"];   
    LOG(info) << "LongTime.third: " << outData["LongTime.third"];
    LOG(info) << "LongTime.fourth: " << outData["LongTime.fourth"];    
    LOG(info) << "String.first: " << outData["String.first"];
    LOG(info) << "String.second: " << outData["String.second"];


    while (std::cin.get() != '\n') {
    }
}

void test_struct()
{
    // 打印struct的偏移值
    std::cout << "Offset of DT_BOOLEAN: " << offsetof(SimpleModel, DT_BOOLEAN) << "\n";
    std::cout << "Offset of DT_BYTE: " << offsetof(SimpleModel, DT_BYTE) << "\n";
    std::cout << "Offset of DT_WORD: " << offsetof(SimpleModel, DT_WORD) << "\n";
    std::cout << "Offset of DT_DWORD: " << offsetof(SimpleModel, DT_DWORD) << "\n";
    std::cout << "Offset of DT_LWORD: " << offsetof(SimpleModel, DT_LWORD) << "\n";
    std::cout << "Offset of DT_SINT: " << offsetof(SimpleModel, DT_SINT) << "\n";
    std::cout << "Offset of DT_USINT: " << offsetof(SimpleModel, DT_USINT) << "\n";
    std::cout << "Offset of DT_INT: " << offsetof(SimpleModel, DT_INT) << "\n";
    std::cout << "Offset of DT_UINT: " << offsetof(SimpleModel, DT_UINT) << "\n";
    std::cout << "Offset of DT_DINT: " << offsetof(SimpleModel, DT_DINT) << "\n";
    std::cout << "Offset of DT_UDINT: " << offsetof(SimpleModel, DT_UDINT) << "\n";
    std::cout << "Offset of DT_LINT: " << offsetof(SimpleModel, DT_LINT) << "\n";
    std::cout << "Offset of DT_ULINT: " << offsetof(SimpleModel, DT_ULINT) << "\n";
    std::cout << "Offset of DT_REAL: " << offsetof(SimpleModel, DT_REAL) << "\n";
    std::cout << "Offset of DT_LREAL: " << offsetof(SimpleModel, DT_LREAL) << "\n";
    std::cout << "Offset of DT_CHAR: " << offsetof(SimpleModel, DT_CHAR) << "\n";
    std::cout << "Offset of DT_CHARSEQ: " << offsetof(SimpleModel, DT_CHARSEQ) << "\n";
    std::cout << "Offset of DT_STRING: " << offsetof(SimpleModel, DT_STRING) << "\n";


    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    std::string xmlContent = R"(<models>
            <struct name="SimpleModel" version="1.0">
                <member name="DT_BOOLEAN" type="DT_BOOLEAN" default="0"/>
                <member name="DT_BYTE" type="DT_BYTE"/>
                <member name="DT_WORD" type="DT_WORD"/>
                <member name="DT_DWORD" type="DT_DWORD"/>
                <member name="DT_LWORD" type="DT_LWORD"/>
                <member name="DT_SINT" type="DT_SINT"/>
                <member name="DT_USINT" type="DT_USINT"/>
                <member name="DT_INT" type="DT_INT"/>
                <member name="DT_UINT" type="DT_UINT"/>
                <member name="DT_DINT" type="DT_DINT"/>
                <member name="DT_UDINT" type="DT_UDINT"/>
                <member name="DT_LINT" type="DT_LINT"/>
                <member name="DT_ULINT" type="DT_ULINT"/>
                <member name="DT_REAL" type="DT_REAL"/>
                <member name="DT_LREAL" type="DT_LREAL"/>
                <member name="DT_CHAR" type="DT_CHAR"/>
                <member name="DT_CHARSEQ" type="DT_CHARSEQ"/>
                <member name="DT_STRING" type="DT_STRING"/>
            </struct>
        </models>)";
    auto &modelParser = dsf::parser::ModelParser::getInstance();
    std::string errorMsg;
    bool ret = modelParser.init(xmlContent, errorMsg);
    std::cout << "modelParser result: " << ret << std::endl;
    dsf::kvpair::KeyValueSerializer kvs = dsf::kvpair::KeyValueSerializer();
    std::unordered_map<std::string, std::string> inData = {
        {"DT_BOOLEAN", "1"},  {"DT_BYTE", "2"},    {"DT_WORD", "4"},     {"DT_DWORD", "5"},
        {"DT_LWORD", "6"},    {"DT_SINT", "7"},    {"DT_USINT", "8"},    {"DT_INT", "9"},
        {"DT_UINT", "10"},    {"DT_DINT", "11"},   {"DT_UDINT", "12"},   {"DT_LINT", "13"},
        {"DT_ULINT", "14"},   {"DT_REAL", "15.0"}, {"DT_LREAL", "16.0"},
        {"DT_CHAR", "7"}, {"DT_CHARSEQ", "18"}, {"DT_STRING", "19"}
        // {"DT_WCHAR", "20"},   {"DT_WCHARSEQ", "21"}, {"DT_WSTRING", "22"}
        };
    std::vector<char> outBuffer;
    ret = kvs.serialize("SimpleModel:1.0", inData, outBuffer);
    std::cout << "Serialize result: " << ret << std::endl;
    std::unordered_map<std::string, std::string> outData;
    ret = kvs.deserialize("SimpleModel:1.0", outBuffer, outData);
    std::cout << "deserialize result: " << ret << std::endl;
    std::cout << "outBuffer size: " << outBuffer.size() << std::endl;
    LOG(info) << "DT_BOOLEAN: " << outData["DT_BOOLEAN"];
    LOG(info) << "DT_BYTE: " << outData["DT_BYTE"];
    LOG(info) << "DT_WORD: " << outData["DT_WORD"];
    LOG(info) << "DT_DWORD: " << outData["DT_DWORD"];
    LOG(info) << "DT_LWORD: " << outData["DT_LWORD"];
    LOG(info) << "DT_SINT: " << outData["DT_SINT"];
    LOG(info) << "DT_USINT: " << outData["DT_USINT"];
    LOG(info) << "DT_INT: " << outData["DT_INT"];
    LOG(info) << "DT_UINT: " << outData["DT_UINT"];
    LOG(info) << "DT_DINT: " << outData["DT_DINT"];
    LOG(info) << "DT_UDINT: " << outData["DT_UDINT"];
    LOG(info) << "DT_LINT: " << outData["DT_LINT"];
    LOG(info) << "DT_ULINT: " << outData["DT_ULINT"];
    LOG(info) << "DT_REAL: " << outData["DT_REAL"];
    LOG(info) << "DT_LREAL: " << outData["DT_LREAL"];
    LOG(info) << "DT_CHAR: " << outData["DT_CHAR"];
    LOG(info) << "DT_CHARSEQ: " << outData["DT_CHARSEQ"];
    LOG(info) << "DT_STRING: " << outData["DT_STRING"];
    // LOG(info) << "DT_WCHAR: " << outData["DT_WCHAR"];
    // LOG(info) << "DT_WCHARSEQ: " << outData["DT_WCHARSEQ"];
    // LOG(info) << "DT_WSTRING: " << outData["DT_WSTRING"];
    // for (auto &item : outData) {
    //     std::cout << item.first << ":" << item.second << std::endl;
    // }
    SimpleModel* simpleVar = (SimpleModel *)outBuffer.data();
    std::cout << "---------------SimpleModel:---------------" << std::endl;
    std::cout << "DT_BOOLEAN: " << simpleVar->DT_BOOLEAN << std::endl;
    std::cout << "DT_BYTE: " << (int)simpleVar->DT_BYTE << std::endl;
    std::cout << "DT_WORD: " << simpleVar->DT_WORD << std::endl;
    std::cout << "DT_DWORD: " << simpleVar->DT_DWORD << std::endl;
    std::cout << "DT_LWORD: " << simpleVar->DT_LWORD << std::endl;
    std::cout << "DT_SINT: " << (int)simpleVar->DT_SINT << std::endl;
    std::cout << "DT_USINT: " << (unsigned int)simpleVar->DT_USINT << std::endl;
    std::cout << "DT_INT: " << simpleVar->DT_INT << std::endl;
    std::cout << "DT_UINT: " << simpleVar->DT_UINT << std::endl;
    std::cout << "DT_DINT: " << simpleVar->DT_DINT << std::endl;
    std::cout << "DT_UDINT: " << simpleVar->DT_UDINT << std::endl;
    std::cout << "DT_LINT: " << simpleVar->DT_LINT << std::endl;
    std::cout << "DT_ULINT: " << simpleVar->DT_ULINT << std::endl;
    std::cout << "DT_REAL: " << simpleVar->DT_REAL << std::endl;
    std::cout << "DT_LREAL: " << simpleVar->DT_LREAL << std::endl;
    std::cout << "DT_CHAR: " << simpleVar->DT_CHAR << std::endl;
    std::cout << "DT_CHARSEQ: " << simpleVar->DT_CHARSEQ << std::endl;
    std::cout << "DT_STRING: " << simpleVar->DT_STRING << std::endl;
    // std::cout << "DT_WCHAR: " << simpleVar->DT_WCHAR << std::endl;
    // std::wcout << L"DT_WCHARSEQ: " << simpleVar->DT_WCHARSEQ << std::endl;
    // std::wcout << L"DT_WSTRING: " << simpleVar->DT_WSTRING << std::endl;

}
int main(int argc, char *argv[])
{
    test_struct();

    return 0;
}