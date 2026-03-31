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

uint32_t count = 5000;

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
        LOG(info) << "FreqChangeCallback: var=" << varName << " newFreq=" << freq;
    });

    // 预缓存 varId
    std::vector<uint32_t> varIds(count);
    for (int i = 0; i < count; ++i)
        varIds[i] = pub.getVarId(("var" + std::to_string(i)).c_str());

    // 预分配 batch items，热循环只更新 data 指针
    std::vector<dsf::ondemand::OnDemandPub::VarWriteItem> batchItems(count);
    std::vector<int> vals(count);
    for (int i = 0; i < count; ++i) {
        vals[i] = i;
        batchItems[i].id = varIds[i];
        batchItems[i].data = &vals[i];
        batchItems[i].size = sizeof(int);
    }

    std::thread setVarThread([&pub, &batchItems]() {
#if defined(__linux__)
        pthread_setname_np(pthread_self(), "setvar");
#endif
        while (true) {
            pub.setVarDataBatch(batchItems.data(), count);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::vector<dsf::ondemand::SubscriptionItem> items;
    std::vector<std::string> unitems;

    const int subscribedCount = 5000;

    for (int i = 0; i < subscribedCount; ++i) {
        std::string varName = "var" + std::to_string(i + 5000);
        items.push_back({varName, static_cast<uint32_t>(10)});
        unitems.push_back(varName);
    }

    sub.subscribe("pubNodeB", items, [](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
        if (vars.empty())
            return;

        LOG(info) << "Callback: batch size=" << vars.size();
    });

    setVarThread.join();

    std::this_thread::sleep_for(std::chrono::seconds(100000));
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
        var.name("var" + std::to_string(i + 5000));
        var.nodeName("pubNodeB");
        var.modelName("int");
        var.size(sizeof(int));
        vars.push_back(std::move(var));
    }
    pub.createVars(vars);
    pub.setFreqChangeCallback([](const std::string &varName, uint32_t freq) {
        LOG(info) << "FreqChangeCallback: var=" << varName << " newFreq=" << freq;
    });

    // 预缓存 varId
    std::vector<uint32_t> varIds(count);
    for (int i = 0; i < count; ++i)
        varIds[i] = pub.getVarId(("var" + std::to_string(i + 5000)).c_str());

    // 预分配 batch items，热循环只更新 data 指针
    std::vector<dsf::ondemand::OnDemandPub::VarWriteItem> batchItems(count);
    std::vector<int> vals(count);
    for (int i = 0; i < count; ++i) {
        vals[i] = i;
        batchItems[i].id = varIds[i];
        batchItems[i].data = &vals[i];
        batchItems[i].size = sizeof(int);
    }

    std::thread setVarThread([&pub, &batchItems]() {
#if defined(__linux__)
        pthread_setname_np(pthread_self(), "setvar");
#endif
        while (true) {
            pub.setVarDataBatch(batchItems.data(), count);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::vector<dsf::ondemand::SubscriptionItem> items;
    std::vector<std::string> unitems;

    const int subscribedCount = 5000;

    for (int i = 0; i < subscribedCount; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, static_cast<uint32_t>(10)});
        unitems.push_back(varName);
    }

    sub.subscribe("pubNodeA", items, [](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
        if (vars.empty())
            return;

        LOG(info) << "Callback: batch size=" << vars.size();
    });

    setVarThread.join();

    std::this_thread::sleep_for(std::chrono::seconds(100000));
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

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
