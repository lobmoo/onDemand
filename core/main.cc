#include <bits/stdint-uintn.h>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include <iomanip>
#include <atomic>
#include "log/logger.h"
#include "ondemand/on_demand_pub.h"
#include "ondemand/on_demand_sub.h"
#include <fastdds/dds/log/Log.hpp>

void publish()
{
    dsf::ondemand::OnDemandPub pub;
    pub.init("pubNode");
    pub.start();
    uint32_t count = 100000;
    std::vector<DSF::Var::Define> vars;
    for (int i = 0; i < count; ++i) {
        DSF::Var::Define var;
        var.name("var" + std::to_string(i));
        var.nodeName("pubNode");
        var.modelName("int");
        vars.push_back(std::move(var));
    }
    pub.createVars(vars);

    pub.setFreqChangeCallback([](const std::string &varName, uint32_t freq) {
        LOG(info) << "FreqChangeCallback: var=" << varName << " newFreq=" << freq;
    });
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::vector<std::string> varDelNames;
    for (int i = 0; i < count; ++i) {
        std::string varName = "var" + std::to_string(i);
        varDelNames.push_back(varName);
    }
    // pub.deleteVars(varDelNames);

    while (true) {
        for (int i = 0; i < count; ++i) {
            std::string varName = "var" + std::to_string(i);
            pub.setVarData(varName.c_str(), &i, sizeof(i));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::this_thread::sleep_for(std::chrono::seconds(100000));
}

void subscribe()
{
    dsf::ondemand::OnDemandSub sub;
    std::string nodeName = "subNode" + std::to_string(getpid());
    sub.init(nodeName);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    sub.start();
    while (sub.getTotalReceivedVars() < 5) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::vector<dsf::ondemand::SubscriptionItem> items;
    std::vector<std::string> unitems;
    for (int i = 0; i < 100000; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, 250});
        unitems.push_back(varName);
    }
    /*延迟/丢包统计*/
    constexpr int64_t kPeriodMs = 250;  // 订阅周期 500ms
    constexpr int64_t kPrintIntervalMs = 1000; // 打印间隔 1s
    constexpr uint64_t kExpectedBatch = kPrintIntervalMs / kPeriodMs; // 期望批次数
    struct CallbackStats {
        std::atomic<uint64_t> totalCount{0};   // 回调变量总数
        std::atomic<uint64_t> batchCount{0};   // 回调批次数
        std::atomic<int64_t>  latencySumMs{0};
        std::atomic<int64_t>  latencyMaxMs{0};
    };
    auto stats = std::make_shared<CallbackStats>();

    sub.subscribe("pubNode", items,
        [stats](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
        if (vars.empty()) return;
        stats->totalCount.fetch_add(vars.size(), std::memory_order_relaxed);
        stats->batchCount.fetch_add(1, std::memory_order_relaxed);

        /* 延迟: 当前时间 - pub 时间戳 */
        int64_t ts = static_cast<int64_t>(vars[0].timestampNs);
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        int64_t nowNs = static_cast<int64_t>(now.tv_sec) * 1000000000LL + now.tv_nsec;
        int64_t latencyMs = (nowNs - ts) / 1000000;

        stats->latencySumMs.fetch_add(latencyMs, std::memory_order_relaxed);
        int64_t curMax = stats->latencyMaxMs.load(std::memory_order_relaxed);
        while (latencyMs > curMax) {
            if (stats->latencyMaxMs.compare_exchange_weak(curMax, latencyMs,
                    std::memory_order_relaxed))
                break;
        }
    });

    /*定期打印统计*/
    for (int round = 0; round < 600; ++round) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint64_t total  = stats->totalCount.exchange(0, std::memory_order_relaxed);
        uint64_t batch  = stats->batchCount.exchange(0, std::memory_order_relaxed);
        uint64_t loss   = (batch < kExpectedBatch) ? (kExpectedBatch - batch) : 0;
        int64_t  sumMs  = stats->latencySumMs.exchange(0, std::memory_order_relaxed);
        int64_t  maxMs  = stats->latencyMaxMs.exchange(0, std::memory_order_relaxed);
        int64_t  avgMs  = batch > 0 ? sumMs / static_cast<int64_t>(batch) : 0;
        LOG(info) << "[Stats] vars=" << total
                  << " batch=" << batch << "/" << kExpectedBatch
                  << " loss=" << loss
                  << " avgLatency=" << avgMs << "ms"
                  << " maxLatency=" << maxMs << "ms";
    }
}

void subscribe2()
{
    dsf::ondemand::OnDemandSub sub;
    std::string nodeName = "subNode" + std::to_string(getpid());
    sub.init(nodeName);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    sub.start();
    while (sub.getTotalReceivedVars() < 5) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::vector<dsf::ondemand::SubscriptionItem> items;
    std::vector<std::string> unitems;
    for (int i = 0; i < 50000; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, 500});
        unitems.push_back(varName);
    }
    sub.subscribe("pubNode", items, [](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
        LOG(info) << "Callback2: batch size=" << vars.size();
    });
    std::this_thread::sleep_for(std::chrono::seconds(10));
    // sub.unsubscribe("pubNode", unitems);
    //sub.stop();
    std::this_thread::sleep_for(std::chrono::seconds(1000000));
}

int main(int argc, char **argv)
{
    // eprosima::fastdds::dds::Log::SetVerbosity(eprosima::fastdds::dds::Log::Kind::Info);
    Logger::GetInstance()->Init("log/1.log", Logger::console, Logger::info, 10, 3);
    LOG(info) << "start on demand demo";

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " sub/pub" << std::endl;
        return -1;
    }

    if (strcmp(argv[1], "sub") == 0) {
        subscribe();
    } else if (strcmp(argv[1], "sub2") == 0) {
        subscribe2();
    } else if (strcmp(argv[1], "pub") == 0) {
        publish();
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
