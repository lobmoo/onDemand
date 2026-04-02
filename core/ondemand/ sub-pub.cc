#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include <iomanip>
#include <atomic>
#include <charconv>
#include <mutex>
#include <string_view>
#include <cstdlib>
#include <pthread.h>
#include "log/logger.h"
#include "ondemand/on_demand_pub.h"
#include "ondemand/on_demand_sub.h"

uint32_t count = 1000;  // 每个节点发布5000个变量

void dataNodeA()
{
    dsf::ondemand::OnDemandPub pub;
    dsf::ondemand::OnDemandSub sub;
    pub.init("pubNodeA");
    sub.init("subNodeA");
    pub.start();
    sub.start();

    std::vector<DSF::Var::Define> vars;
    for (int i = 0; i < count; ++i) {
        DSF::Var::Define var;
        var.name("var" + std::to_string(i));
        var.nodeName("pubNodeA");
        var.modelName("int");
        var.size(sizeof(int));
        vars.push_back(std::move(var));
    }
    pub.createVars(vars);
    pub.setFreqChangeCallback([](const std::string &varName, uint32_t freq) {
        LOG(info) << "NodeA FreqChangeCallback: var=" << varName << " newFreq=" << freq;
    });

    // 预缓存 varId
    std::vector<uint32_t> varIds(count);
    for (int i = 0; i < count; ++i)
        varIds[i] = pub.getVarId(("var" + std::to_string(i)).c_str());

    // 预分配 batch items，热循环只更新 data 指针
    std::vector<dsf::ondemand::OnDemandPub::VarWriteItem> batchItems(count);
    std::vector<int> vals(count);
    for (int i = 0; i < count; ++i) {
        vals[i] = i * 100;  
        batchItems[i].id = varIds[i];
        batchItems[i].data = &vals[i];
        batchItems[i].size = sizeof(int);
    }

    std::thread setVarThread([&pub, &batchItems, &vals]() {
#if defined(__linux__)
        pthread_setname_np(pthread_self(), "setvar_A");
#endif
        while (true) {
            // 每次递增值
            for (int i = 0; i < count; ++i) {
                vals[i]++;
            }
            pub.setVarDataBatch(batchItems.data(), count);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    
    std::vector<dsf::ondemand::SubscriptionItem> items;
    for (int i = 0; i < count; ++i) {
        std::string varName = "var" + std::to_string(i + count);  // var3, var4, var5
        items.push_back({varName, static_cast<uint32_t>(2000)});
    }

    sub.subscribe("pubNodeB", items, [](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
        if (vars.empty())
            return;

        LOG(info) << "NodeA received from B: batch size=" << vars.size();
        // for (const auto &var : vars) {
        //     if (var.size == sizeof(int)) {
        //         int value = *reinterpret_cast<const int*>(var.data);
        //         LOG(info) << "  " << var.varName << " = " << value;
        //     }
        // }
    });

    setVarThread.detach();  // 让线程在后台运行

    // 保持节点运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void dataNodeB()
{
    dsf::ondemand::OnDemandPub pub;
    dsf::ondemand::OnDemandSub sub;
    pub.init("pubNodeB");
    sub.init("subNodeB");
    pub.start();
    sub.start();

    std::vector<DSF::Var::Define> vars;
    for (int i = 0; i < count; ++i) {
        DSF::Var::Define var;
        var.name("var" + std::to_string(i + count));  // var3, var4, var5
        var.nodeName("pubNodeB");
        var.modelName("int");
        var.size(sizeof(int));
        vars.push_back(std::move(var));
    }
    pub.createVars(vars);
    pub.setFreqChangeCallback([](const std::string &varName, uint32_t freq) {
        LOG(info) << "NodeB FreqChangeCallback: var=" << varName << " newFreq=" << freq;
    });

    // 预缓存 varId
    std::vector<uint32_t> varIds(count);
    for (int i = 0; i < count; ++i)
        varIds[i] = pub.getVarId(("var" + std::to_string(i + count)).c_str());

    // 预分配 batch items，热循环只更新 data 指针
    std::vector<dsf::ondemand::OnDemandPub::VarWriteItem> batchItems(count);
    std::vector<int> vals(count);
    for (int i = 0; i < count; ++i) {
        vals[i] = (i + count) * 100;  // var3=300, var4=400, var5=500
        batchItems[i].id = varIds[i];
        batchItems[i].data = &vals[i];
        batchItems[i].size = sizeof(int);
    }

    std::thread setVarThread([&pub, &batchItems, &vals]() {
#if defined(__linux__)
        pthread_setname_np(pthread_self(), "setvar_B");
#endif
        while (true) {
            // 每次递增值
            for (int i = 0; i < count; ++i) {
                vals[i]++;
            }
            pub.setVarDataBatch(batchItems.data(), count);
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    });


    std::vector<dsf::ondemand::SubscriptionItem> items;
    for (int i = 0; i < count; ++i) {
        std::string varName = "var" + std::to_string(i);  // var0, var1, var2
        items.push_back({varName, static_cast<uint32_t>(2000)});
    }

    sub.subscribe("pubNodeA", items, [](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
        if (vars.empty())
            return;

        LOG(info) << "NodeB received from A: batch size=" << vars.size();
        // for (const auto &var : vars) {
        //     if (var.size == sizeof(int)) {
        //         int value = *reinterpret_cast<const int*>(var.data);
        //         LOG(info) << "  " << var.varName << " = " << value;
        //     }
        // }
    });

    setVarThread.detach();  // 让线程在后台运行

    // 保持节点运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char **argv)
{
    Logger::GetInstance()->Init("log/1.log", Logger::console, Logger::info, 10, 3);
    LOG(info) << "start on demand demo";

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " A/B" << std::endl;
        return -1;
    }

    if (strcmp(argv[1], "A") == 0) {
        dataNodeA();
    } else if (strcmp(argv[1], "B") == 0) {
        dataNodeB();
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }

    return 0;
}
