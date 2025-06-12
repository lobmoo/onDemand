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

int main(int argc, char *argv[])
{
    /* 1. 初始化日志和xml内容 */
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    std::string xmlContent = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model.xml");
    auto &serializer = dsf::ngvs::KeyValueSerializer::getInstance();

    /* 2. 定义输入数据和输出空间 */
    // 基本元素
    _Float32 first = 3.4028235e+38f;  
    int64_t second = 9223372036854775807LL;
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
    _Float32 complex_member3_complex_member6_first = 11.11f;
    int32_t complex_member3_complex_member6_second = 1212121212;
    int8_t complex_member3_complex_member6_third = 13;
    // 继承结构
    _Float32 complex_member4_first = 14.f;
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
    char *buf1 = (char*)&first, *buf2 = (char*)&second, *buf3 = (char*)&complex_member2_third, *buf4 = (char*)&complex_member2_complex_member3_fourth, 
         *buf5 = (char*)&complex_member2_complex_member3_longarray0, *buf6 = (char*)&complex_member2_complex_member3_longarray1,
         *buf7 = (char*)&complex_member2_complex_member3_longarray2, *buf8 = (char*)&complex_member2_complex_member3_longarray3,
         *buf9 = (char*)&complex_member2_complex_member3_longarray4, *buf10 = (char*)&fourth,
         *buf11 = (char*)&complex_member3_complex_member6_first, *buf12 = (char*)&complex_member3_complex_member6_second,
         *buf13 = (char*)&complex_member3_complex_member6_third, *buf14 = (char*)&complex_member4_first,  *buf15 = (char*)&complex_member4_second, 
         *buf16 = (char*)&complex_member4_complex_member_fourth, *buf17 = (char*)&complex_member4_complex_member_longarray0,
         *buf18 = (char*)&complex_member4_complex_member_longarray1, *buf19 = (char*)&complex_member4_complex_member_longarray2,
         *buf20 = (char*)&complex_member4_complex_member_longarray3, *buf21 = (char*)&complex_member4_complex_member_longarray4,
         *buf22 = (char*)&short_sequence0, *buf23 = (char*)&short_sequence1, *buf24 = (char*)&short_sequence2, *buf25 = (char*)&short_sequence3,
         *buf26 = (char*)&short_sequence4; 
    std::unordered_map<std::string, char *> data = {
        {"first", buf1}, {"second", buf2}, {"complex_member2.third", buf3}, {"complex_member2.complex_member3.fourth", buf4},
        {"complex_member2.complex_member3.long_array[0]", buf5}, {"complex_member2.complex_member3.long_array[1]", buf6},
        {"complex_member2.complex_member3.long_array[2]", buf7}, {"complex_member2.complex_member3.long_array[3]", buf8},
        {"complex_member2.complex_member3.long_array[4]", buf9}, {"fourth", buf10}, {"complex_member3.complex_member6.first", buf11},
        {"complex_member3.complex_member6.second", buf12}, {"complex_member3.complex_member6.third", buf13}, {"complex_member4.first", buf14},
        {"complex_member4.second", buf15}, {"complex_member4.complex_member.fourth", buf16}, {"complex_member4.complex_member.long_array[0]", buf17},
        {"complex_member4.complex_member.long_array[1]", buf18}, {"complex_member4.complex_member.long_array[2]", buf19},
        {"complex_member4.complex_member.long_array[3]", buf20}, {"complex_member4.complex_member.long_array[4]", buf21},
        {"short_sequence[0]", buf22}, {"short_sequence[1]", buf23}, {"short_sequence[2]", buf24}, 
        {"short_sequence[3]", buf25}, {"short_sequence[4]", buf26}};
    std::vector<char> outBuff;

    /* 3.序列化 */
    serializer.serialize(xmlContent, "StructArray:1.0", data, outBuff);

    
    /* 4.输出序列化结果 */
    // TestNormalModel* model = (TestNormalModel*)(outBuff.data());
    // LOG(info) << "first: " << model->first << " second: " << model->second << " third: " << (int)(model->third);


    /* 5.反序列化 */
    // std::unordered_map<std::string, char *> outData;
    // serializer.deserialize(xmlContent, "InnerModel33:1.0", outBuff, outData);

    // _Float32 first_ = *reinterpret_cast<_Float32*>(outData["first"]);
    // int64_t second_ = *reinterpret_cast<int64_t*>(outData["second"]);
    // int32_t complex_member2_third_ = *reinterpret_cast<int32_t*>(outData["complex_member2.third"]);
    // int32_t complex_member2_complex_member3_fourth_ = *reinterpret_cast<int32_t*>(outData["complex_member2.complex_member3.fourth"]);
    // int32_t complex_member2_complex_member3_longarray0_ = *reinterpret_cast<int32_t*>(outData["complex_member2.complex_member3.long_array[0]"]);
    // int32_t complex_member2_complex_member3_longarray1_ = *reinterpret_cast<int32_t*>(outData["complex_member2.complex_member3.long_array[1]"]);
    // int32_t complex_member2_complex_member3_longarray2_ = *reinterpret_cast<int32_t*>(outData["complex_member2.complex_member3.long_array[2]"]);
    // int32_t complex_member2_complex_member3_longarray3_ = *reinterpret_cast<int32_t*>(outData["complex_member2.complex_member3.long_array[3]"]);
    // int32_t complex_member2_complex_member3_longarray4_ = *reinterpret_cast<int32_t*>(outData["complex_member2.complex_member3.long_array[4]"]);
    // int8_t fourth_ = *reinterpret_cast<int8_t*>(outData["fourth"]);
    // _Float32 complex_member3_complex_member6_first_ = *reinterpret_cast<_Float32*>(outData["complex_member3.complex_member6.first"]);
    // int32_t complex_member3_complex_member6_second_ = *reinterpret_cast<int32_t*>(outData["complex_member3.complex_member6.second"]);
    // int8_t complex_member3_complex_member6_third_ = *reinterpret_cast<int8_t*>(outData["complex_member3.complex_member6.third"]);
    // _Float32 complex_member4_first_ = *reinterpret_cast<_Float32*>(outData["complex_member4.first"]);
    // int64_t complex_member4_second_ = *reinterpret_cast<int64_t*>(outData["complex_member4.second"]);
    // int32_t complex_member4_complex_member_fourth_ = *reinterpret_cast<int32_t*>(outData["complex_member4.complex_member.fourth"]);
    // int32_t complex_member4_complex_member_longarray0_ = *reinterpret_cast<int32_t*>(outData["complex_member4.complex_member.long_array[0]"]);
    // int32_t complex_member4_complex_member_longarray1_ = *reinterpret_cast<int32_t*>(outData["complex_member4.complex_member.long_array[1]"]);
    // int32_t complex_member4_complex_member_longarray2_ = *reinterpret_cast<int32_t*>(outData["complex_member4.complex_member.long_array[2]"]);
    // int32_t complex_member4_complex_member_longarray3_ = *reinterpret_cast<int32_t*>(outData["complex_member4.complex_member.long_array[3]"]);
    // int32_t complex_member4_complex_member_longarray4_ = *reinterpret_cast<int32_t*>(outData["complex_member4.complex_member.long_array[4]"]);
    // int32_t short_sequence0_ = *reinterpret_cast<int32_t*>(outData["short_sequence[0]"]);
    // int32_t short_sequence1_ = *reinterpret_cast<int32_t*>(outData["short_sequence[1]"]);
    // int32_t short_sequence2_ = *reinterpret_cast<int32_t*>(outData["short_sequence[2]"]);
    // int32_t short_sequence3_ = *reinterpret_cast<int32_t*>(outData["short_sequence[3]"]);
    // int32_t short_sequence4_ = *reinterpret_cast<int32_t*>(outData["short_sequence[4]"]);
    // LOG(info) << "first: " << first_;
    // LOG(info) << "second: " << second_;
    // LOG(info) << "complex_member2.third: " << complex_member2_third_;
    // LOG(info) << "complex_member2_complex_member3.fourth: " << complex_member2_complex_member3_fourth_;
    // LOG(info) << "complex_member2_complex_member3.longarray[0]: " << complex_member2_complex_member3_longarray0_;
    // LOG(info) << "complex_member2_complex_member3.longarray[1]: " << complex_member2_complex_member3_longarray1_;
    // LOG(info) << "complex_member2_complex_member3.longarray[2]: " << complex_member2_complex_member3_longarray2_;
    // LOG(info) << "complex_member2_complex_member3.longarray[3]: " << complex_member2_complex_member3_longarray3_;
    // LOG(info) << "complex_member2_complex_member3.longarray[4]: " << complex_member2_complex_member3_longarray4_;
    // LOG(info) << "complex_member3.complex_member6.first: " << complex_member3_complex_member6_first_;
    // LOG(info) << "complex_member3.complex_member6.second: " << complex_member3_complex_member6_second_;
    // LOG(info) << "complex_member3.complex_member6.third: " << (int)complex_member3_complex_member6_third_;
    // LOG(info) << "complex_member4.first: " << complex_member4_first_;
    // LOG(info) << "complex_member4.second: " << complex_member4_second_;
    // LOG(info) << "complex_member4.complex_member.fourth: " << complex_member4_complex_member_fourth_;
    // LOG(info) << "complex_member4.complex_member.long_array[0]: " << complex_member4_complex_member_longarray0_;
    // LOG(info) << "complex_member4.complex_member.long_array[1]: " << complex_member4_complex_member_longarray1_;
    // LOG(info) << "complex_member4.complex_member.long_array[2]: " << complex_member4_complex_member_longarray2_;
    // LOG(info) << "complex_member4.complex_member.long_array[3]: " << complex_member4_complex_member_longarray3_;
    // LOG(info) << "complex_member4.complex_member.long_array[4]: " << complex_member4_complex_member_longarray4_;
    // LOG(info) << "fourth: " << (int)fourth_;
    // LOG(info) << "short_sequence[0]: " << short_sequence0_;
    // LOG(info) << "short_sequence[1]: " << short_sequence1_;
    // LOG(info) << "short_sequence[2]: " << short_sequence2_;
    // LOG(info) << "short_sequence[3]: " << short_sequence3_;
    // LOG(info) << "short_sequence[4]: " << short_sequence4_;
    

    while (std::cin.get() != '\n') {
    }
    return 0;
}