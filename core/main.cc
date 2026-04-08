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
#include <charconv>
#include <mutex>
#include <string_view>
#include <cstdlib>
#include <pthread.h>
#include "log/logger.h"
#include "ondemand/on_demand_pub.h"
#include "ondemand/on_demand_sub.h"

uint32_t count = 1000;

static int parse_var_index(std::string_view varName)
{
    constexpr std::string_view kPrefix = "var";
    if (varName.size() <= kPrefix.size() || varName.compare(0, kPrefix.size(), kPrefix) != 0) {
        return -1;
    }

    int idx = -1;
    const char *begin = varName.data() + kPrefix.size();
    const char *end = varName.data() + varName.size();
    auto [ptr, ec] = std::from_chars(begin, end, idx);
    if (ec != std::errc() || ptr != end || idx < 0) {
        return -1;
    }
    return idx;
}
void publish()
{
    dsf::ondemand::OnDemandPub pub;
    pub.init("pubNode");
    pub.start();
    std::vector<DSF::Var::Define> vars;
    for (int i = 0; i < count; ++i) {
        DSF::Var::Define var;
        var.name("var" + std::to_string(i));
        var.nodeName("pubNode");
        var.modelName("int");
        var.size(sizeof(int));
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
    setVarThread.join();

    std::this_thread::sleep_for(std::chrono::seconds(100000));
}

void subscribe()
{
#if defined(__linux__)
    pthread_setname_np(pthread_self(), "submain");
#endif
    dsf::ondemand::OnDemandSub sub;
    std::string nodeName = "subNode" + std::to_string(getpid());
    sub.init(nodeName);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    sub.start();

    // while (sub.getTotalReceivedVars() < count - 1) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }

    LOG(critical) << "Waiting for vars... received=" << sub.getTotalReceivedVars();
    std::vector<dsf::ondemand::SubscriptionItem> items;
    std::vector<std::string> unitems;

    // auto varinfo = sub.getAvailableVars();
    // LOG(critical) << "Waiting for vars... received=" << varinfo.size() << " nodes";
    // for (const auto &var : varinfo) {
    //     LOG(info) << "node name = " << var.first;
    //     for (auto &v : var.second) {
    //         LOG(info) << "    var name = " << v;
    //     }
    // }

    const int subscribedCount = 1000;

    /* 延迟/丢包统计参数 */
    constexpr int64_t kPeriodMs = 10;          // 订阅周期
    constexpr int64_t kPrintIntervalMs = 1000; // 打印间隔 1s
    constexpr int64_t kBucketSize = 20;        // bucket 数量，与 ONDEMAND_BUCKET_SIZE 一致
    constexpr uint64_t kExpectedBatch = (kPrintIntervalMs / kPeriodMs) * kBucketSize;

    for (int i = 0; i < subscribedCount; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, static_cast<uint32_t>(kPeriodMs)});
        unitems.push_back(varName);
    }

    struct CallbackStats {
        std::atomic<uint64_t> totalCount{0};
        std::atomic<uint64_t> batchCount{0};
        std::atomic<int64_t> latencySumMs{0};
        std::atomic<int64_t> latencyMaxMs{0};

        std::mutex gapMutex;
        std::vector<uint64_t> lastSeenRecvNs;
        uint64_t gapSampleCount{0};
        uint64_t gapOverrunCount{0}; // gap > 1.5x period
        int64_t gapSumMs{0};
        int64_t gapDevSumMs{0};
        int64_t gapDevMaxMs{0};

        explicit CallbackStats(size_t n) : lastSeenRecvNs(n, 0) {}
    };
    auto stats = std::make_shared<CallbackStats>(subscribedCount);

    auto nodeAObservedTsNs = std::make_shared<std::atomic<uint64_t>>(0);
    constexpr uint64_t kExpectedIntervalNs = 10ULL * 1000ULL * 1000ULL;
    constexpr uint64_t kToleranceNs = 2ULL * 1000ULL * 1000ULL;

    constexpr uint64_t kWarnThresholdNs = kExpectedIntervalNs + kToleranceNs;
    sub.subscribe("pubNode", items,
                  [nodeAObservedTsNs](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                      if (vars.empty())
                          return;
                      for (const auto &var : vars) {
                          if (var.varName == "var0") {
                              LOG(info) << "NodeA received " << var.varName
                                        << "   timestamp=" << var.timestampNs;
                              uint64_t currentTsNs = var.timestampNs;
                              uint64_t prevTsNs = nodeAObservedTsNs->exchange(
                                  currentTsNs, std::memory_order_acq_rel);
                              if (prevTsNs != 0 && currentTsNs > prevTsNs) {
                                  uint64_t deltaNs = currentTsNs - prevTsNs;
                                  if (deltaNs > kWarnThresholdNs) {
                                      LOG(warning)
                                          << "NodeA observed " << var.varName << " timestamp gap "
                                          << (deltaNs / 1000000ULL)
                                          << "ms (threshold=" << (kWarnThresholdNs / 1000000ULL)
                                          << "ms)";
                                  }
                              }
                          }
                      }
                  });

    // sub.unsubscribe("pubNode", unitems);
    // sub.stop();
    std::this_thread::sleep_for(std::chrono::seconds(100000));
}

void subscribe2()
{
#if defined(__linux__)
    pthread_setname_np(pthread_self(), "submain2");
#endif
    dsf::ondemand::OnDemandSub sub;
    std::string nodeName = "subNode" + std::to_string(getpid());
    sub.init(nodeName);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    sub.start();

    std::vector<dsf::ondemand::SubscriptionItem> items;
    std::vector<std::string> unitems;
    for (int i = 0; i < count; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, 500});
        unitems.push_back(varName);
    }
    sub.subscribe("pubNode", items, [](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
        // LOG(info) << "Callback2: batch size=" << vars.size();
    });
    std::this_thread::sleep_for(std::chrono::seconds(10));
    // sub.unsubscribe("pubNode", unitems);
    // sub.stop();
    std::this_thread::sleep_for(std::chrono::seconds(1000000));
}

int main(int argc, char **argv)
{
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
