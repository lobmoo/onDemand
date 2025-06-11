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
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::debug, 60, 5);
    // std::string xmlContent = readXmlFile("/home/wwk/workspaces/test_demo/sample/NGVS/model.xml");
    // dsf::ngvs::NgvsSerializer serializer;
    // std::vector<char> outBuffer;
    // if (!serializer.serialize(xmlContent, "InnerModel:1.0", outBuffer)) {
    //     LOG(error) << "Serialization failed";
    //     return 1;
    // }
    std::string xmlContent = readXmlFile("/home/weiqb/src/test_demo/sample/NGVS/model.xml");
    dsf::ngvs::KeyValueSerializer serializer;
    _Float32 first = 3.4028235e+38f;
    int64_t second = 9223372036854775807LL;
    int8_t third = 127;

    char *buf1 = (char*)&first, *buf2 = (char*)&second, *buf3 = (char*)&third;
    std::unordered_map<std::string, char *> data = {
        {"first", buf1},
        {"second", buf2},
        {"third", buf3}
    };
    serializer.serialize(xmlContent, "InnerModel33:1.0", data);

    //取得serializer中的buffer，然后强转成struct
    const char * buffer = serializer.buffer();
    TestNormalModel* model = (TestNormalModel*)buffer;
    LOG(info) << "first: " << model->first << " second: " << model->second << " third: " << (int)(model->third);
    while (std::cin.get() != '\n') {
    }

    return 0;
}