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
#pragma pack(push, 4)  // 强制对齐为4字节
struct TestNormalModel {
    _Float32 first;   // 4 字节
    int64_t second;   // 8 字节，允许非8字节对齐
    int8_t third;     // 1 字节
};
#pragma pack(pop)

int main(int argc, char *argv[])
{
    /* 1. 初始化日志和xml内容 */
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    std::string xmlContent = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model.xml");
    auto &serializer = dsf::ngvs::KeyValueSerializer::getInstance();

    /* 2. 定义输入数据和输出空间 */
    _Float32 first = 3.4028235e+38f;
    int64_t second = 9223372036854775807LL;
    int8_t third = 127;

    char *buf1 = (char*)&first, *buf2 = (char*)&second, *buf3 = (char*)&third;
    std::unordered_map<std::string, char *> data = {
        {"first", buf1},
        {"second", buf2},
        {"third", buf3}
    };
    std::vector<char> outBuff;

    /* 3.序列化 */
    serializer.serialize(xmlContent, "InnerModel:1.0", data, outBuff);

    
    /* 4.输出序列化结果 */
    // TestNormalModel* model = (TestNormalModel*)(outBuff.data());
    // LOG(info) << "first: " << model->first << " second: " << model->second << " third: " << (int)(model->third);


    /* 5.反序列化 */
    // std::unordered_map<std::string, char *> outData;
    // serializer.deserialize(xmlContent, "InnerModel33:1.0", outBuff, outData);

    // _Float32 first_ = *reinterpret_cast<_Float32*>(outData["first"]);
    // int64_t second_ = *reinterpret_cast<int64_t*>(outData["second"]);
    // int8_t third_ = *reinterpret_cast<int8_t*>(outData["third"]);
    // LOG(info) << "first: " << first_ << " second: " << second_ << " third: " << (int)third_;

    while (std::cin.get() != '\n') {
    }
    return 0;
}