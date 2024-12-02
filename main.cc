#include "BS_thread_pool.hpp" // BS::thread_pool
#include <chrono>             // std::chrono
#include <iostream>           // std::cout
#include <thread>             // std::this_thread
#include <cstring>



typedef struct
{
    uint16_t uType;
    uint32_t result;
    char *pData;
} DataPackge;


int main()
{
    char *pData = new char[1024];
    DataPackge b{};
    b.uType = 1;
    b.result = 2;
    char *p = "1234567";
    b.pData = p;
    memcpy(pData, &b, sizeof(DataPackge));


    DataPackge a{};
    a = *(DataPackge *)pData;
    printf("uType:%d,result:%d,pData:%s\n", a.uType, a.result, a.pData);

}