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
    for (int i = 0; i < 50000; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, 500});
        unitems.push_back(varName);
    }
    /*延迟/丢包统计*/
    constexpr int kVarCount = 50000;
    constexpr int64_t kPeriodNs = 500000000LL; // 订阅周期 1s
    struct CallbackStats {
        std::atomic<uint64_t> totalCount{0};
        std::atomic<uint64_t> lossCount{0};    // 两次pub时间戳间隔>周期 → 丢包
        std::atomic<int64_t>  latencySumUs{0};
        std::atomic<int64_t>  latencyMaxUs{0};
    };
    auto stats = std::make_shared<CallbackStats>();
    // per-var 记录上次收到的 pub 时间戳 (ns)
    auto lastTs = std::shared_ptr<int64_t[]>(new int64_t[kVarCount]());

    sub.subscribe("pubNode", items,
        [stats, lastTs, kPeriodNs](const std::string &varName,
            const void *data, size_t size, uint64_t timestampNs) {
        stats->totalCount.fetch_add(1, std::memory_order_relaxed);


        int idx = std::atoi(varName.c_str() + 3); // "varN" -> N
        int64_t ts = static_cast<int64_t>(timestampNs);
        int64_t prev = lastTs[idx];
        lastTs[idx] = ts;

        /* 丢包检测: 两次pub时间戳间隔 / 订阅周期, 四舍五入后 > 1 则丢包 */
        if (prev > 0) {
            int64_t gapNs = ts - prev;
            int64_t rounds = (gapNs + kPeriodNs / 2) / kPeriodNs;
            if (rounds > 1)
                stats->lossCount.fetch_add(rounds - 1, std::memory_order_relaxed);
        }

        /* 延迟: 当前时间 - pub端时间戳 */
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        int64_t nowNs = static_cast<int64_t>(now.tv_sec) * 1000000000LL + now.tv_nsec;
        int64_t latencyUs = (nowNs - ts) / 1000;

        stats->latencySumUs.fetch_add(latencyUs, std::memory_order_relaxed);

        int64_t curMax = stats->latencyMaxUs.load(std::memory_order_relaxed);
        while (latencyUs > curMax) {
            if (stats->latencyMaxUs.compare_exchange_weak(curMax, latencyUs,
                    std::memory_order_relaxed))
                break;
        }
    });

    /*定期打印统计*/
    for (int round = 0; round < 600; ++round) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint64_t total = stats->totalCount.exchange(0, std::memory_order_relaxed);
        uint64_t loss  = stats->lossCount.exchange(0, std::memory_order_relaxed);
        int64_t  sumUs = stats->latencySumUs.exchange(0, std::memory_order_relaxed);
        int64_t  maxUs = stats->latencyMaxUs.exchange(0, std::memory_order_relaxed);
        int64_t  avgUs = total > 0 ? sumUs / static_cast<int64_t>(total) : 0;
        LOG(info) << "[Stats] callbacks=" << total
                  << " loss=" << loss
                  << " avgLatency=" << avgUs << "us"
                  << " maxLatency=" << maxUs << "us";
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
    sub.subscribe("pubNode", items, [](const std::string &varName, const void *data, size_t size, uint64_t timestampNs) {
        LOG(info) << "Callback2: var=" << varName << " size=" << size;
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
