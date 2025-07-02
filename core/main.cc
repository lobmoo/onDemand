#include <log/logger.h>
#include <string>
#include <iostream>

#pragma pack(push, 4)
struct  __attribute__((aligned(4)))stu
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
};
#pragma pack(pop)


struct __attribute__((aligned(4))) stu1
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
};


struct __attribute__((packed, aligned(4))) stu2
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
};

struct stu3
{
    uint8_t a;
    uint64_t b;
    uint8_t c;
};

// #pragma pack(push, 4)
// __attribute__((packed, aligned(4)))

struct __attribute__((aligned(4))) SimpleModel {
    bool DT_BOOLEAN;          // 布尔，1字节
    // uint8_t  _pad0;
    int8_t DT_BYTE;           // 8位，1字节
    // uint8_t  _pad1; 
    int16_t DT_WORD;          // 16位，2字节
    int32_t DT_DWORD;         // 32位，4字节
    int64_t DT_LWORD;         // 64位，8字节
    int8_t DT_SINT;           // 8位有符号整数，1字节
    // uint8_t  _pad2;
    uint8_t DT_USINT;         // 8位无符号整数，1字节
    // uint8_t  _pad3;
    int16_t DT_INT;           // 16位有符号整数，2字节
    uint16_t DT_UINT;         // 16位无符号整数，2字节
    int32_t DT_DINT;          // 32位有符号整数，4字节
    uint32_t DT_UDINT;        // 32位无符号整数，4字节
    int64_t DT_LINT;          // 64位有符号整数，8字节
    uint64_t DT_ULINT;        // 64位无符号整数，8字节

    //48
    float DT_REAL;            // 32位浮点数，4字节
    double DT_LREAL;          // 64位浮点数，8字节
    char DT_CHAR;             // 单字节字符，1字节
    // uint8_t  _pad4;
    char DT_CHARSEQ[82];      // 单字节字符数组，默认N=80，占82字节
    char DT_STRING[82];       // 字符串，假设与DT_CHARSEQ相同
};
// #pragma pack(pop)


int main(int argc, char *argv[])
{
    Logger::Instance().Init("");

    LOG(info) << "stu size: " << sizeof(stu);
    LOG(info) << "stu1 size: " << sizeof(stu1); 
    LOG(info) << "stu2 size: " << sizeof(stu2);

    
  
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
    return 0;
}