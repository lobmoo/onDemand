#include <bits/stdint-uintn.h>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <atomic>
#include <charconv>
#include <cstring>
#include <pthread.h>
#include "log/logger.h"
#include "ondemand/on_demand_pub.h"
#include "ondemand/on_demand_sub.h"

uint32_t count = 100000;

constexpr uint64_t kExpectedPeriodMs = 100ULL;
constexpr uint64_t kTolerancePercent = 10ULL;
constexpr uint64_t kToleranceMs = (kExpectedPeriodMs * kTolerancePercent) / 100ULL;
constexpr uint64_t kUpperThresholdMs = kExpectedPeriodMs + kToleranceMs;
constexpr uint64_t kLowerThresholdMs = kExpectedPeriodMs - kToleranceMs;
constexpr uint32_t kStatsWindowRecv = 100U;

struct BucketStats {
    uint64_t last_ts_ms = 0;
    uint32_t recv_count = 0;
    uint32_t lost_count = 0;
    uint32_t init_packet_count = 0; // Skip initial packets until stable
    bool has_baseline = false;      // true after receiving 1st real packet
};

static std::unordered_map<std::string, uint32_t> g_repr_var_to_bucket = {
    {"var9702", 0},  {"var9675", 1},  {"var9700", 2},  {"var9686", 3},  {"var9698", 4},
    {"var9684", 5},  {"var9678", 6},  {"var9682", 7},  {"var9676", 8},  {"var9680", 9},
    {"var9674", 10}, {"var9703", 11}, {"var9687", 12}, {"var9701", 13}, {"var9685", 14},
    {"var9699", 15}, {"var9683", 16}, {"var9679", 17}, {"var9681", 18}, {"var9677", 19},
};

static std::unordered_map<uint32_t, BucketStats> g_bucket_stats;

struct OwnedBatchItem {
    std::string var_name;
    std::string type_name;
    std::string type_version;
    std::vector<uint8_t> data;
    uint32_t data_size = 0;
    uint64_t timestamp_ms = 0;
    int32_t blobType = 0;
};

struct BatchEvent {
    std::string node_name;
    std::vector<OwnedBatchItem> items;
};

static std::deque<BatchEvent> g_bucket_event_queue;
static std::mutex g_bucket_event_mutex;
static std::condition_variable g_bucket_event_cv;
static std::atomic<bool> g_bucket_worker_started{false};

static void processBucketSample(uint32_t bucket_id, uint64_t ts)
{
    BucketStats &st = g_bucket_stats[bucket_id];

    // Skip initial packets until system is stable
    constexpr uint32_t kSkipInitPackets = 10U;
    if (st.init_packet_count < kSkipInitPackets) {
        st.init_packet_count++;
        st.last_ts_ms = ts;
        return;
    }

    // First real packet: just establish baseline, no delta calc
    if (!st.has_baseline) {
        st.last_ts_ms = ts;
        st.has_baseline = true;
        return;
    }

    // Second+ real packet: calculate delta
    if (ts > st.last_ts_ms) {
        uint64_t delta = ts - st.last_ts_ms;
        uint32_t lost = 0;

        // Only count as lost packet if delta > 100ms
        if (delta > kExpectedPeriodMs) {
            uint64_t expected_count = (delta + kToleranceMs / 2) / kExpectedPeriodMs;
            lost = (expected_count <= 1) ? 0U : static_cast<uint32_t>(expected_count - 1U);
        }

        st.recv_count += 1;
        st.lost_count += lost;
        if (st.recv_count >= kStatsWindowRecv) {
            uint32_t total = st.recv_count + st.lost_count;
            double rate = (total == 0) ? 0.0 : (100.0 * st.lost_count / total);
            LOG(critical) << "| bucket=" << bucket_id << " | recv=" << st.recv_count
                          << " | lost=" << st.lost_count << " | total=" << total
                          << " | loss_rate=" << rate << "%";
            st.recv_count = 0;
            st.lost_count = 0;
        }
    }

    st.last_ts_ms = ts;
}

static void ensureBucketWorkerStarted()
{
    bool expected = false;
    if (!g_bucket_worker_started.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([]() {
        auto local_start = std::chrono::steady_clock::now();
        constexpr uint64_t kWarmupMs = 5000ULL;

        while (true) {
            BatchEvent batch;
            {
                std::unique_lock<std::mutex> lock(g_bucket_event_mutex);
                g_bucket_event_cv.wait(lock, []() { return !g_bucket_event_queue.empty(); });
                batch = std::move(g_bucket_event_queue.front());
                g_bucket_event_queue.pop_front();
            }

            auto now = std::chrono::steady_clock::now();
            uint64_t elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - local_start).count();
            if (elapsed_ms < kWarmupMs) {
                continue;
            }

            for (const auto &item : batch.items) {
                auto it = g_repr_var_to_bucket.find(item.var_name);
                if (it == g_repr_var_to_bucket.end()) {
                    continue;
                }
                processBucketSample(it->second, item.timestamp_ms);
            }
        }
    }).detach();
}

void dataCallback(const std::vector<dsf::ondemand::VarCallbackData> &vars)
{
    if (vars.empty()) {
        return;
    }

    ensureBucketWorkerStarted();

    BatchEvent batch;
    if (!vars.front().nodeName.empty()) {
        batch.node_name.assign(vars.front().nodeName.data(), vars.front().nodeName.size());
    }
    batch.items.reserve(vars.size());

    for (const auto &src : vars) {
        OwnedBatchItem dst;
        if (!src.varName.empty()) {
            dst.var_name.assign(src.varName.data(), src.varName.size());
        }
        if (!src.varType.empty()) {
            dst.type_name.assign(src.varType.data(), src.varType.size());
        }
        if (!src.type_version.empty()) {
            dst.type_version.assign(src.type_version.data(), src.type_version.size());
        }
        if (src.data != nullptr && src.size > 0) {
            const uint8_t *ptr = static_cast<const uint8_t *>(src.data);
            dst.data.assign(ptr, ptr + src.size);
        }
        dst.data_size = static_cast<uint32_t>(src.size);
        dst.timestamp_ms = src.timestampNs / 1000000ULL;
        dst.blobType = static_cast<int32_t>(src.blobType);
        batch.items.push_back(std::move(dst));
    }

    if (batch.items.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_bucket_event_mutex);
        g_bucket_event_queue.push_back(std::move(batch));
    }
    g_bucket_event_cv.notify_one();
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
        vals[i] = i + 30;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
    for (int i = 0; i < count; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, 100});
        unitems.push_back(varName);
    }
    LOG(info) << "Subscribing representative vars for packet-loss stats, count=" << items.size();

    // auto varinfo = sub.getAvailableVars();
    // LOG(critical) << "Waiting for vars... received=" << varinfo.size() << " nodes";
    // for (const auto &var : varinfo) {
    //     LOG(info) << "node name = " << var.first;
    //     for (auto &v : var.second) {
    //         LOG(info) << "    var name = " << v;
    //     }
    // }

    sub.subscribe("pubNode", items, dataCallback);

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
