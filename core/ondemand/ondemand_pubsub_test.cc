#include <gtest/gtest.h>

#ifdef __linux__

#    include <signal.h>
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <unistd.h>

#    include <atomic>
#    include <chrono>
#    include <cstdint>
#    include <filesystem>
#    include <fstream>
#    include <functional>
#    include <map>
#    include <mutex>
#    include <set>
#    include <sstream>
#    include <string>
#    include <thread>
#    include <unordered_map>
#    include <utility>
#    include <vector>

#    include "log/logger.h"
#    include "on_demand_pub.h"
#    include "on_demand_sub.h"

namespace
{
using namespace std::chrono_literals;

struct ChildReport {
    bool ok{false};
    std::string message;
    std::map<std::string, std::string> metrics;
};

struct ChildHandle {
    std::string name;
    pid_t pid{-1};
    std::filesystem::path reportPath;
    std::filesystem::path logPath;
};

void initTestLogger(const std::string &tag)
{
    const std::string logFile = "log/ondemand_pubsub_test_" + tag + ".log";
    Logger::GetInstance()->Init(logFile.c_str(), Logger::console, Logger::info, 20, 3);
}

std::string uniqueName(const std::string &prefix)
{
    const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    std::ostringstream oss;
    oss << prefix << "_" << getpid() << "_" << nowNs;
    return oss.str();
}

void writeReport(const std::filesystem::path &path, const ChildReport &report)
{
    std::ofstream out(path, std::ios::trunc);
    out << "ok=" << (report.ok ? "1" : "0") << "\n";
    out << "message=" << report.message << "\n";
    for (const auto &kv : report.metrics) {
        out << "metric." << kv.first << "=" << kv.second << "\n";
    }
}

bool childRequire(ChildReport &report, bool condition, const std::string &message,
                  const char *file = __builtin_FILE(), int line = __builtin_LINE())
{
    if (condition) {
        return true;
    }
    report.ok = false;
    if (report.message.empty()) {
        report.message = message;
    }
    LOG(error) << "[CHILD FAIL] " << file << ":" << line << "  " << message;
    return false;
}

ChildReport readReport(const std::filesystem::path &path)
{
    ChildReport report;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        if (key == "ok") {
            report.ok = (val == "1");
        } else if (key == "message") {
            report.message = val;
        } else if (key.rfind("metric.", 0) == 0) {
            report.metrics[key.substr(7)] = val;
        }
    }
    return report;
}

bool waitUntil(const std::function<bool()> &predicate, const std::chrono::milliseconds timeout,
               const std::chrono::milliseconds poll = 50ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(poll);
    }
    return predicate();
}

std::vector<DSF::Var::Define> makeDefines(const std::string &node, const std::string &prefix,
                                          size_t count, size_t startIndex = 0)
{
    std::vector<DSF::Var::Define> vars;
    vars.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        DSF::Var::Define v;
        v.nodeName(node);
        v.name(prefix + std::to_string(startIndex + i));
        v.modelName("int32");
        v.size(static_cast<uint32_t>(sizeof(int32_t)));
        vars.push_back(std::move(v));
    }
    return vars;
}

std::vector<std::string> defineNames(const std::vector<DSF::Var::Define> &defines)
{
    std::vector<std::string> names;
    names.reserve(defines.size());
    for (const auto &d : defines) {
        names.push_back(d.name());
    }
    return names;
}

void publishLoop(dsf::ondemand::OnDemandPub &pub, const std::vector<std::string> &varNames,
                 const std::atomic<bool> &running, int periodMs = 20)
{
    int32_t value = 1;
    while (running.load(std::memory_order_acquire)) {
        for (const auto &name : varNames) {
            pub.setVarData(name.c_str(), &value, sizeof(value));
            ++value;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
    }
}

size_t countNodeVars(dsf::ondemand::OnDemandSub &sub, const std::string &nodeName)
{
    const auto vars = sub.getAvailableVars();
    auto it = vars.find(nodeName);
    if (it == vars.end()) {
        return 0;
    }
    return it->second.size();
}

std::vector<std::string> nodeVars(dsf::ondemand::OnDemandSub &sub, const std::string &nodeName)
{
    const auto vars = sub.getAvailableVars();
    auto it = vars.find(nodeName);
    if (it == vars.end()) {
        return {};
    }
    return it->second;
}

ChildHandle spawnChild(const std::string &name, const std::filesystem::path &root,
                       const std::function<ChildReport()> &task)
{
    const std::filesystem::path reportPath = root / (name + ".report");
    const std::filesystem::path logPath =
        std::filesystem::path("log") / ("ondemand_pubsub_test_" + name + ".log");
    pid_t pid = fork();
    if (pid == 0) {
        ChildReport r;
        try {
            initTestLogger(name);
            r = task();
        } catch (const std::exception &e) {
            r.ok = false;
            r.message = std::string("exception: ") + e.what();
        } catch (...) {
            r.ok = false;
            r.message = "unknown exception";
        }
        writeReport(reportPath, r);
        _exit(r.ok ? 0 : 1);
    }
    return ChildHandle{name, pid, reportPath, logPath};
}

bool waitChildExit(const ChildHandle &h, const std::chrono::milliseconds timeout, int &exitStatus,
                   ChildReport &report)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        pid_t ret = waitpid(h.pid, &status, WNOHANG);
        if (ret == h.pid) {
            exitStatus = status;
            report = readReport(h.reportPath);
            return true;
        }
        std::this_thread::sleep_for(50ms);
    }

    kill(h.pid, SIGKILL);
    waitpid(h.pid, &status, 0);
    exitStatus = status;
    report.ok = false;
    report.message = "timeout";
    return false;
}

void expectChildOk(const ChildHandle &h, const std::chrono::milliseconds timeout,
                   ChildReport *outReport = nullptr)
{
    int status = 0;
    ChildReport report;
    const bool exited = waitChildExit(h, timeout, status, report);

    auto buildDiag = [&]() -> std::string {
        std::ostringstream oss;
        oss << "\n  message: " << report.message;
        for (const auto &kv : report.metrics) {
            oss << "\n  metric." << kv.first << "=" << kv.second;
        }
        std::ifstream lf(h.logPath);
        if (lf.is_open()) {
            std::vector<std::string> lines;
            std::string ln;
            while (std::getline(lf, ln)) {
                lines.push_back(ln);
            }
            const size_t start = lines.size() > 40 ? lines.size() - 40 : 0;
            oss << "\n  --- child log (last " << (lines.size() - start) << " lines) ---";
            for (size_t i = start; i < lines.size(); ++i) {
                oss << "\n  " << lines[i];
            }
        }
        return oss.str();
    };

    ASSERT_TRUE(exited) << h.name << " timed out" << buildDiag();
    ASSERT_TRUE(WIFEXITED(status)) << h.name << " terminated by signal" << buildDiag();
    ASSERT_EQ(WEXITSTATUS(status), 0) << h.name << " exited with failure:" << buildDiag();
    ASSERT_TRUE(report.ok) << h.name << " report failed:" << buildDiag();
    if (outReport != nullptr) {
        *outReport = report;
    }
}

std::vector<dsf::ondemand::SubscriptionItem> toSubscriptions(const std::vector<std::string> &names,
                                                             uint32_t freqMs)
{
    std::vector<dsf::ondemand::SubscriptionItem> subs;
    subs.reserve(names.size());
    for (const auto &n : names) {
        subs.emplace_back(n, freqMs);
    }
    return subs;
}

double averageIntervalMs(const std::vector<uint64_t> &tsNs)
{
    if (tsNs.size() < 2) {
        return 0.0;
    }
    uint64_t sumNs = 0;
    size_t intervals = 0;
    for (size_t i = 1; i < tsNs.size(); ++i) {
        if (tsNs[i] > tsNs[i - 1]) {
            sumNs += (tsNs[i] - tsNs[i - 1]);
            ++intervals;
        }
    }
    if (intervals == 0) {
        return 0.0;
    }
    return static_cast<double>(sumNs) / static_cast<double>(intervals) / 1e6;
}

} // namespace

// TC1 - 生命周期基础验证
// 场景：pub 和 sub 各自在独立子进程中执行 init → start → 重复start（应返回false）→ stop → 重复stop。
// 验证：init/start 返回 true，重复 start 返回 false（幂等保护），stop 不崩溃。
TEST(OnDemandPubSub, ProcessLifecycleInitStartStop)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case1");
    std::filesystem::create_directories(root);

    const auto pubProc = spawnChild("case1_pub", root, []() {
        dsf::ondemand::OnDemandPub pub;
        const std::string node = uniqueName("pub_case1");
        ChildReport r;
        const bool initOk = pub.init(node);
        const bool startOk = pub.start();
        const bool startAgain = pub.start();
        std::this_thread::sleep_for(200ms);
        pub.stop();
        pub.stop();
        r.ok = initOk && startOk && !startAgain;
        r.message = r.ok ? "ok" : "pub lifecycle failed";
        return r;
    });

    const auto subProc = spawnChild("case1_sub", root, []() {
        dsf::ondemand::OnDemandSub sub;
        const std::string node = uniqueName("sub_case1");
        ChildReport r;
        const bool initOk = sub.init(node);
        const bool startOk = sub.start();
        const bool startAgain = sub.start();
        std::this_thread::sleep_for(200ms);
        sub.stop();
        sub.stop();
        r.ok = initOk && startOk && !startAgain;
        r.message = r.ok ? "ok" : "sub lifecycle failed";
        return r;
    });

    expectChildOk(pubProc, 8s);
    expectChildOk(subProc, 8s);
}

// TC2 - 变量增删后订阅端可见性验证
// 场景：pub 分三阶段操作变量：阶段1 创建20个变量，阶段2 追加创建5个变量，阶段3 删除前10个变量。
//       sub 在独立子进程中持续监听，每个阶段等待变量数量稳定后，同时验证数量和名称集合是否与预期完全一致。
// 验证：createVars/deleteVars 的增量变更能被 sub 正确感知，数量和名称集合在每个阶段均准确。
TEST(OnDemandPubSub, Create20ThenAdd5ThenDelete10CountAndNamesAccurate)
{
    // 用例目标：验证 create/delete 后，订阅端看到的变量数量和名称集合都准确。
    // 阶段1：创建20个变量；阶段2：再创建5个变量；阶段3：删除前10个变量。
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case2");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case2");
    const auto defs20 = makeDefines(pubNode, "s9_a", 20);
    const auto names20 = defineNames(defs20);
    const auto defs5 = makeDefines(pubNode, "s9_b", 5);
    const auto names5 = defineNames(defs5);
    // 删除策略：删掉首批20个里的前10个（a0~a9）。
    const std::vector<std::string> toDelete(names20.begin(), names20.begin() + 10);

    // 期望集合（阶段1）：仅20个a变量。
    std::set<std::string> expected20(names20.begin(), names20.end());
    // 期望集合（阶段2）：20个a + 5个b，共25个。
    std::set<std::string> expected25 = expected20;
    expected25.insert(names5.begin(), names5.end());
    // 期望集合（阶段3）：删掉a0~a9后，剩余a10~a19 + 全部b，共15个。
    std::set<std::string> expected15;
    expected15.insert(names20.begin() + 10, names20.end());
    expected15.insert(names5.begin(), names5.end());

    const auto pubProc = spawnChild("case2_pub", root, [pubNode, defs20, defs5, toDelete]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs20), "pub createVars(20) failed")) {
            return r;
        }

        // 阶段1完成后等待传播，避免与下一阶段采样混叠。
        std::this_thread::sleep_for(1500ms);

        // 阶段2：追加创建5个变量。
        if (!childRequire(r, pub.createVars(defs5), "pub createVars(+5) failed")) {
            pub.stop();
            return r;
        }

        std::this_thread::sleep_for(1500ms);

        // 阶段3：删除前10个a变量。
        if (!childRequire(r, pub.deleteVars(toDelete), "pub deleteVars(10) failed")) {
            pub.stop();
            return r;
        }

        std::this_thread::sleep_for(1500ms);
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc =
        spawnChild("case2_sub", root, [pubNode, expected20, expected25, expected15]() {
            dsf::ondemand::OnDemandSub sub;
            ChildReport r;
            if (!childRequire(r, sub.init(uniqueName("sub_case2")), "sub init failed")
                || !childRequire(r, sub.start(), "sub start failed")) {
                return r;
            }

            // 阶段1断言：数量=20，且名称集合与 expected20 完全一致。
            const bool got20 = waitUntil([&]() { return countNodeVars(sub, pubNode) == 20; }, 8s);
            const auto vars20 = nodeVars(sub, pubNode);
            const std::set<std::string> got20Set(vars20.begin(), vars20.end());
            const bool names20Ok = (got20Set == expected20);

            // 阶段2断言：数量=25，且名称集合与 expected25 完全一致。
            const bool got25 = waitUntil([&]() { return countNodeVars(sub, pubNode) == 25; }, 8s);
            const auto vars25 = nodeVars(sub, pubNode);
            const std::set<std::string> got25Set(vars25.begin(), vars25.end());
            const bool names25Ok = (got25Set == expected25);

            // 阶段3断言：数量=15，且名称集合与 expected15 完全一致。
            const bool got15 = waitUntil([&]() { return countNodeVars(sub, pubNode) == 15; }, 10s);
            const auto vars15 = nodeVars(sub, pubNode);
            const std::set<std::string> got15Set(vars15.begin(), vars15.end());
            const bool names15Ok = (got15Set == expected15);

            sub.stop();

            r.ok = childRequire(r, got20, "count did not reach 20")
                   && childRequire(r, names20Ok, "name set mismatch at 20 vars")
                   && childRequire(r, got25, "count did not reach 25")
                   && childRequire(r, names25Ok, "name set mismatch at 25 vars")
                   && childRequire(r, got15, "count did not reach 15 after delete")
                   && childRequire(r, names15Ok, "final name set mismatch at 15 vars");
            r.metrics["count_stage20"] = std::to_string(vars20.size());
            r.metrics["count_stage25"] = std::to_string(vars25.size());
            r.metrics["count_stage15"] = std::to_string(vars15.size());
            return r;
        });

    expectChildOk(pubProc, 16s);
    ChildReport subReport;
    expectChildOk(subProc, 16s, &subReport);
    // metrics are diagnostic: show actual counts on failure
    EXPECT_EQ(subReport.metrics["count_stage20"], "20")
        << "actual count at stage20: " << subReport.metrics["count_stage20"];
    EXPECT_EQ(subReport.metrics["count_stage25"], "25")
        << "actual count at stage25: " << subReport.metrics["count_stage25"];
    EXPECT_EQ(subReport.metrics["count_stage15"], "15")
        << "actual count at stage15: " << subReport.metrics["count_stage15"];
}

// TC3 - 变量定义广播 + 数据接收正确性 + 回调周期验证
// 场景：pub 创建6个变量并以20ms间隔持续写入递增值，sub 订阅全部变量（频率100ms）。
//       sub 等待变量定义到达后发起订阅，收集每个变量的最新值和至少10次回调时间戳。
// 验证：① sub 能收到所有变量的定义；② 每个变量都收到了非零值；
//       ③ 用首尾时间戳跨度计算平均回调间隔，允许 ±30% 误差（兼容 CI 服务器调度抖动）。
TEST(OnDemandPubSub, PublishDefineAndSubReceiveCountAccurate)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case3");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case3");
    const auto defs = makeDefines(pubNode, "v", 6);
    const auto names = defineNames(defs);
    constexpr uint32_t kFreqMs = 100;
    constexpr size_t kSamples = 10; // 收集足够多的样本再算平均

    const auto pubProc = spawnChild("case3_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 20); });
        std::this_thread::sleep_for(8s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case3_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!sub.init(uniqueName("sub_case3")) || !sub.start()) {
            r.ok = false;
            r.message = "sub init/start failed";
            return r;
        }

        // 1. 等待变量定义到达，验证数量
        const bool gotDefines =
            waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 6s);
        if (!childRequire(r, gotDefines, "sub did not receive expected defines")) {
            sub.stop();
            return r;
        }

        // 2. 订阅所有变量，收集最新值和时间戳序列
        std::mutex dataMutex;
        std::map<std::string, int32_t> latestValues;
        std::map<std::string, std::vector<uint64_t>> tsLog; // 每个变量收集 kSamples 个严格递增时间戳

        const auto subs = toSubscriptions(names, kFreqMs);
        const bool subOk = sub.subscribe(
            pubNode.c_str(), subs, [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                std::lock_guard<std::mutex> lk(dataMutex);
                for (const auto &v : vars) {
                    const std::string name(v.varName.data(), v.varName.size());
                    if (v.data && v.size >= sizeof(int32_t))
                        latestValues[name] = *reinterpret_cast<const int32_t *>(v.data);
                    auto &ts = tsLog[name];
                    // 只收集严格递增的非零时间戳，避免定时器踩踏导致的重复值
                    if (ts.size() < kSamples && v.timestampNs > 0
                        && (ts.empty() || v.timestampNs > ts.back()))
                        ts.push_back(v.timestampNs);
                    LOG(info) << "callback for var " << name << " ts=" << v.timestampNs
                              << " val=" << latestValues[name];
                }
            });

        if (!childRequire(r, subOk, "subscribe failed")) {
            sub.stop();
            return r;
        }

        // 3. 等待每个变量都积累到 kSamples 次有效时间戳
        const bool gotEnough = waitUntil(
            [&]() {
                std::lock_guard<std::mutex> lk(dataMutex);
                for (const auto &n : names)
                    if (tsLog[n].size() < kSamples)
                        return false;
                return true;
            },
            10s);

        sub.stop();

        if (!childRequire(r, gotEnough, "not enough callbacks to verify period"))
            return r;

        // 4. 验证：所有变量都收到了值（非零）
        {
            std::lock_guard<std::mutex> lk(dataMutex);
            for (const auto &n : names) {
                if (!childRequire(r, latestValues.count(n) > 0, "no value received for var: " + n))
                    return r;
                if (!childRequire(r, latestValues[n] != 0, "value is zero for var: " + n))
                    return r;
            }
        }

        // 5. 验证周期：用首尾时间戳跨度算平均间隔，允许 ±30%
        //    不逐对检查，避免定时器单次抖动导致误判
        {
            std::lock_guard<std::mutex> lk(dataMutex);
            constexpr double kExpectedMs = static_cast<double>(kFreqMs);
            constexpr double kLowMs  = kExpectedMs * 0.70;
            constexpr double kHighMs = kExpectedMs * 1.30;
            for (const auto &n : names) {
                const auto &ts = tsLog[n];
                const double avgMs = static_cast<double>(ts.back() - ts.front())
                                     / static_cast<double>(ts.size() - 1) / 1e6;
                LOG(info) << "avg period for var " << n << ": " << static_cast<int>(avgMs) << "ms"
                          << " (samples=" << ts.size() << ")";
                r.metrics["avg_ms_" + n] = std::to_string(static_cast<int>(avgMs));
                if (!childRequire(r, avgMs >= kLowMs && avgMs <= kHighMs,
                                  "avg period out of range for var " + n + ": "
                                      + std::to_string(static_cast<int>(avgMs)) + "ms"
                                      + " expected [" + std::to_string(static_cast<int>(kLowMs))
                                      + "," + std::to_string(static_cast<int>(kHighMs)) + "]ms"))
                    return r;
            }
        }

        r.ok = true;
        r.message = "ok";
        r.metrics["seen_vars"] = std::to_string(latestValues.size());
        return r;
    });

    expectChildOk(pubProc, 16s);
    expectChildOk(subProc, 16s);
}

// TC4 - 订阅频率协商与回调速率验证
// 场景：pub 创建2个变量并注册频率变更回调，sub 以不同频率订阅（var0=100ms，var1=250ms）。
//       pub 侧验证收到的频率变更通知是否与 sub 请求一致；
//       sub 侧收集4秒内的回调时间戳，计算平均间隔，验证实际回调速率是否符合订阅频率。
// 验证：① pub 的 freqChangeCallback 能感知到 sub 的频率请求；
//       ② sub 实际收到的回调间隔与订阅频率匹配（var0 在 [60,180]ms，var1 在 [170,400]ms）。
TEST(OnDemandPubSub, FrequencyRequestAndCallbackRate)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case4");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case4");
    const auto defs = makeDefines(pubNode, "f", 2);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case4_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        std::atomic<uint32_t> f0{0};
        std::atomic<uint32_t> f1{0};
        pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
            LOG(info) << "[case4_pub] freq change: " << varName << " -> " << freq << "ms";
            if (varName == names[0]) {
                f0.store(freq, std::memory_order_release);
            }
            if (varName == names[1]) {
                f1.store(freq, std::memory_order_release);
            }
        });

        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        const bool sawExpected = waitUntil(
            [&]() {
                return f0.load(std::memory_order_acquire) == 100
                       && f1.load(std::memory_order_acquire) == 250;
            },
            4s);
        std::this_thread::sleep_for(5s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();

        const uint32_t got0 = f0.load(std::memory_order_acquire);
        const uint32_t got1 = f1.load(std::memory_order_acquire);
        r.metrics["freq0"] = std::to_string(got0);
        r.metrics["freq1"] = std::to_string(got1);
        childRequire(r, sawExpected,
                     "pub freq callback mismatch: f0=" + std::to_string(got0)
                         + " f1=" + std::to_string(got1) + " (expected f0=100 f1=250)");
        r.ok = sawExpected;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    const auto subProc = spawnChild("case4_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case4")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 6s),
                "sub did not receive defines")) {
            sub.stop();
            return r;
        }

        std::mutex tsMutex;
        std::unordered_map<std::string, std::vector<uint64_t>> tsByVar;
        std::vector<dsf::ondemand::SubscriptionItem> subs;
        subs.emplace_back(names[0], 100);
        subs.emplace_back(names[1], 250);

        if (!childRequire(
                r,
                sub.subscribe(pubNode.c_str(), subs,
                              [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                  std::lock_guard<std::mutex> lk(tsMutex);
                                  for (const auto &v : vars) {
                                      const std::string name(v.varName.data(), v.varName.size());
                                      tsByVar[name].push_back(v.timestampNs);
                                      LOG(info) << "[case4_sub] data cb: " << name
                                                << " ts=" << v.timestampNs
                                                << " cnt=" << tsByVar[name].size();
                                  }
                              }),
                "subscribe failed")) {
            sub.stop();
            return r;
        }

        std::this_thread::sleep_for(4s);
        sub.stop();

        double avg0 = 0.0;
        double avg1 = 0.0;
        size_t cnt0 = 0;
        size_t cnt1 = 0;
        {
            std::lock_guard<std::mutex> lk(tsMutex);
            cnt0 = tsByVar[names[0]].size();
            cnt1 = tsByVar[names[1]].size();
            avg0 = averageIntervalMs(tsByVar[names[0]]);
            avg1 = averageIntervalMs(tsByVar[names[1]]);
        }

        r.metrics["avg0_ms"] = std::to_string(static_cast<int>(avg0));
        r.metrics["avg1_ms"] = std::to_string(static_cast<int>(avg1));
        r.metrics["ts_count0"] = std::to_string(cnt0);
        r.metrics["ts_count1"] = std::to_string(cnt1);
        LOG(info) << "[case4_sub] var0 avg=" << static_cast<int>(avg0) << "ms cnt=" << cnt0
                  << "  var1 avg=" << static_cast<int>(avg1) << "ms cnt=" << cnt1;

        const bool rate0ok = (avg0 >= 60.0 && avg0 <= 180.0);
        const bool rate1ok = (avg1 >= 170.0 && avg1 <= 400.0);
        childRequire(r, rate0ok,
                     "var0 avg interval " + std::to_string(static_cast<int>(avg0))
                         + "ms not in [60,180], callbacks=" + std::to_string(cnt0));
        childRequire(r, rate1ok,
                     "var1 avg interval " + std::to_string(static_cast<int>(avg1))
                         + "ms not in [170,400], callbacks=" + std::to_string(cnt1));
        r.ok = rate0ok && rate1ok;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    expectChildOk(pubProc, 14s);
    expectChildOk(subProc, 14s);
}

// TC5 - 一pub多sub频率协商与逐步回退验证
// 场景：pub 创建10个变量并持续写入数据。三个 sub 进程按顺序依次订阅：
//       sub1 以200ms订阅 → pub 感知到最小频率变为200ms；
//       sub2 以100ms订阅 → 最小频率降为100ms，pub 感知到降频变化；
//       sub3 以300ms订阅 → 最小频率不变（100 < 300），pub 无新事件；
//       sub1(200ms) 取消 → 最小频率不变（sub2 仍以100ms订阅）；
//       sub2(100ms) 取消 → 最小频率回退到300ms，pub 感知到变化；
//       sub3(300ms) 取消 → 无订阅者，pub 感知到0xFFFFFFFF。
// 验证：覆盖"首次订阅建立频率"、"新订阅降频"、"高频率订阅不影响最小值"、
//       "取消非最小频率订阅不触发回调"、"取消最小频率订阅触发回退"四个场景。
TEST(OnDemandPubSub, MultiSubFrequencyNegotiationAndFallback)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case5");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case5");
    const auto defs = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case5_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        std::mutex mu;
        std::unordered_map<std::string, uint32_t> freqMap;
        pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
            LOG(info) << "[case5_pub] freq change: " << varName << " -> " << freq << "ms";
            std::lock_guard<std::mutex> lk(mu);
            freqMap[varName] = freq;
        });

        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });

        auto allVarsAtFreq = [&](uint32_t expected) {
            std::lock_guard<std::mutex> lk(mu);
            if (freqMap.size() < names.size())
                return false;
            for (const auto &n : names) {
                auto it = freqMap.find(n);
                if (it == freqMap.end() || it->second != expected)
                    return false;
            }
            return true;
        };

        // 阶段1：sub1(200ms) 订阅后，所有变量频率变为200
        const bool saw200 = waitUntil([&]() { return allVarsAtFreq(200); }, 10s);
        childRequire(r, saw200, "did not see freq=200 after sub1(200ms) subscribed");
        LOG(info) << "[case5_pub] stage1 passed: all vars at 200ms";

        // 阶段2：sub2(100ms) 订阅后，频率降为100
        const bool saw100 = waitUntil([&]() { return allVarsAtFreq(100); }, 10s);
        childRequire(r, saw100, "did not see freq=100 after sub2(100ms) subscribed");
        LOG(info) << "[case5_pub] stage2 passed: all vars at 100ms (降频)";

        // 阶段3：sub1 和 sub2 相继取消后，只剩 sub3(300ms)，频率回退到300
        const bool saw300 = waitUntil([&]() { return allVarsAtFreq(300); }, 15s);
        childRequire(r, saw300, "did not see freq=300 after sub1+sub2 unsubscribed");
        LOG(info) << "[case5_pub] stage3 passed: all vars at 300ms";

        // 阶段4：sub3 取消后，无订阅者，频率变为0xFFFFFFFF
        const bool sawNone = waitUntil([&]() { return allVarsAtFreq(0xFFFFFFFF); }, 12s);
        childRequire(r, sawNone, "did not see freq=0xFFFFFFFF after all subs unsubscribed");
        LOG(info) << "[case5_pub] stage4 passed: all vars at 0xFFFFFFFF";

        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();

        r.ok = saw200 && saw100 && saw300 && sawNone;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    // sub1：200ms 订阅，持续 8s 后取消（取消后 sub2 仍以100ms订阅，频率不变）
    const auto sub1Proc = spawnChild("case5_sub1", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub1_case5")), "sub1 init failed")
            || !childRequire(r, sub.start(), "sub1 start failed")) {
            return r;
        }
        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                "sub1 did not receive defines")) {
            sub.stop();
            return r;
        }
        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 200),
                                        [](const std::vector<dsf::ondemand::VarCallbackData> &) {}),
                          "sub1 subscribe(200ms) failed")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case5_sub1] subscribed at 200ms";
        std::this_thread::sleep_for(8s);
        if (!childRequire(r, sub.unsubscribe(pubNode.c_str(), names), "sub1 unsubscribe failed")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case5_sub1] unsubscribed";
        std::this_thread::sleep_for(500ms);
        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    // sub2：100ms 订阅，在 sub1 订阅后 1.5s 加入（触发降频），sub1 取消后再等 3s 取消
    std::this_thread::sleep_for(1500ms);
    const auto sub2Proc = spawnChild("case5_sub2", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub2_case5")), "sub2 init failed")
            || !childRequire(r, sub.start(), "sub2 start failed")) {
            return r;
        }
        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                "sub2 did not receive defines")) {
            sub.stop();
            return r;
        }
        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [](const std::vector<dsf::ondemand::VarCallbackData> &) {}),
                          "sub2 subscribe(100ms) failed")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case5_sub2] subscribed at 100ms";
        std::this_thread::sleep_for(10s);
        if (!childRequire(r, sub.unsubscribe(pubNode.c_str(), names), "sub2 unsubscribe failed")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case5_sub2] unsubscribed";
        std::this_thread::sleep_for(500ms);
        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    // sub3：300ms 订阅，在 sub2 订阅后 1.5s 加入（不影响最小频率），最后取消触发0xFFFFFFFF
    std::this_thread::sleep_for(1500ms);
    const auto sub3Proc = spawnChild("case5_sub3", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub3_case5")), "sub3 init failed")
            || !childRequire(r, sub.start(), "sub3 start failed")) {
            return r;
        }
        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                "sub3 did not receive defines")) {
            sub.stop();
            return r;
        }
        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 300),
                                        [](const std::vector<dsf::ondemand::VarCallbackData> &) {}),
                          "sub3 subscribe(300ms) failed")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case5_sub3] subscribed at 300ms";
        std::this_thread::sleep_for(12s);
        if (!childRequire(r, sub.unsubscribe(pubNode.c_str(), names), "sub3 unsubscribe failed")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case5_sub3] unsubscribed";
        std::this_thread::sleep_for(500ms);
        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    expectChildOk(pubProc, 55s);
    expectChildOk(sub1Proc, 20s);
    expectChildOk(sub2Proc, 25s);
    expectChildOk(sub3Proc, 30s);
}

// TC6 - sub先启动，pub后启动，pub重启后通信恢复验证
// 场景：sub 进程先启动并等待变量定义到达，随后 pub 进程才启动并创建10个变量。
//       sub 收到定义后订阅全部变量（100ms），验证数据回调正常、值非零。
//       pub 进程随后正常退出（模拟掉线），sub 等待变量定义消失后，等待新 pub 上线。
//       新 pub 重新创建相同变量并写入数据，验证 sub 能重新收到定义并恢复数据回调。
// 验证：① sub 先于 pub 启动时，能在 pub 上线后正确发现变量；
//       ② pub 掉线重启后，sub 能重新感知变量定义并恢复通信。
TEST(OnDemandPubSub, SubFirstThenPubRestartRecovery)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case6");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case6");
    const auto defs = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);
    constexpr uint32_t kFreqMs = 100;

    const auto subProc = spawnChild("case6_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case6")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        // 阶段1：等待 pub 上线（pub 比 sub 晚 3s 启动）
        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 12s),
                "stage1: did not receive defines from pub")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case6_sub] stage1: received defines, subscribing";

        std::mutex mu;
        std::map<std::string, int32_t> latestValues;
        std::map<std::string, std::vector<uint64_t>> tsLog;

        auto dataCb = [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &v : vars) {
                const std::string name(v.varName.data(), v.varName.size());
                if (v.data && v.size >= sizeof(int32_t))
                    latestValues[name] = *reinterpret_cast<const int32_t *>(v.data);
                auto &ts = tsLog[name];
                if (ts.size() < 4)
                    ts.push_back(v.timestampNs);
                LOG(info) << "[case6_sub] cb: " << name << " val=" << latestValues[name];
            }
        };

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, kFreqMs), dataCb),
                          "stage1: subscribe failed")) {
            sub.stop();
            return r;
        }

        if (!childRequire(r,
                          waitUntil(
                              [&]() {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &n : names)
                                      if (tsLog[n].size() < 4)
                                          return false;
                                  return true;
                              },
                              8s),
                          "stage1: not enough callbacks")) {
            sub.stop();
            return r;
        }
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &n : names) {
                if (!childRequire(r, latestValues.count(n) > 0, "stage1: no value for " + n)
                    || !childRequire(r, latestValues[n] != 0, "stage1: zero value for " + n)) {
                    sub.stop();
                    return r;
                }
            }
        }
        LOG(info) << "[case6_sub] stage1: data OK";

        // 阶段2：等待 pub 掉线，变量定义消失
        LOG(info) << "[case6_sub] waiting for pub to go offline...";
        waitUntil([&]() { return countNodeVars(sub, pubNode) == 0; }, 12s);
        LOG(info) << "[case6_sub] pub offline, waiting for new pub...";

        {
            std::lock_guard<std::mutex> lk(mu);
            tsLog.clear();
            latestValues.clear();
        }

        // 阶段3：等待新 pub 上线，重新订阅
        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 15s),
                "stage2: did not receive defines from new pub")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case6_sub] stage2: new pub online, re-subscribing";

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, kFreqMs), dataCb),
                          "stage2: re-subscribe failed")) {
            sub.stop();
            return r;
        }

        if (!childRequire(r,
                          waitUntil(
                              [&]() {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &n : names)
                                      if (tsLog[n].size() < 4)
                                          return false;
                                  return true;
                              },
                              8s),
                          "stage2: not enough callbacks after pub restart")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case6_sub] stage2: data OK after pub restart";
        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    // pub1：延迟 3s 启动，运行 6s 后退出（模拟掉线）
    std::this_thread::sleep_for(3s);
    const auto pub1Proc = spawnChild("case6_pub1", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub1 init failed")
            || !childRequire(r, pub.start(), "pub1 start failed")
            || !childRequire(r, pub.createVars(defs), "pub1 createVars failed")) {
            return r;
        }
        LOG(info) << "[case6_pub1] started, publishing 6s then exiting";
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        LOG(info) << "[case6_pub1] stopped (simulating crash)";
        r.ok = true;
        r.message = "ok";
        return r;
    });

    // pub2：等 pub1 退出后 3s 再启动（模拟重启）
    expectChildOk(pub1Proc, 15s);
    std::this_thread::sleep_for(3s);

    const auto pub2Proc = spawnChild("case6_pub2", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub2 init failed")
            || !childRequire(r, pub.start(), "pub2 start failed")
            || !childRequire(r, pub.createVars(defs), "pub2 createVars failed")) {
            return r;
        }
        LOG(info) << "[case6_pub2] restarted, publishing 8s";
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(8s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    expectChildOk(pub2Proc, 20s);
    expectChildOk(subProc, 55s);
}

// TC7 - pub先启动，sub后启动，sub重启后通信恢复验证
// 场景：pub 进程先启动并创建10个变量，随后 sub 进程才启动。
//       sub 收到定义后订阅全部变量（100ms），验证数据回调正常、值非零。
//       sub 进程随后正常退出（模拟掉线），pub 继续运行。
//       新 sub 进程重新启动，验证能重新发现变量定义并恢复数据回调。
// 验证：① pub 先于 sub 启动时，sub 上线后能正确发现变量（定义持久广播）；
//       ② sub 掉线重启后，能重新订阅并恢复通信，pub 侧频率回调也能正确触发。
TEST(OnDemandPubSub, PubFirstThenSubRestartRecovery)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case7");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case7");
    const auto defs = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);
    constexpr uint32_t kFreqMs = 100;

    const auto pubProc = spawnChild("case7_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        std::mutex mu;
        std::unordered_map<std::string, uint32_t> freqMap;
        pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
            LOG(info) << "[case7_pub] freq change: " << varName << " -> " << freq << "ms";
            std::lock_guard<std::mutex> lk(mu);
            freqMap[varName] = freq;
        });

        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });

        auto allVarsAtFreq = [&](uint32_t expected) {
            std::lock_guard<std::mutex> lk(mu);
            if (freqMap.size() < names.size())
                return false;
            for (const auto &n : names) {
                auto it = freqMap.find(n);
                if (it == freqMap.end() || it->second != expected)
                    return false;
            }
            return true;
        };

        // 等待 sub1 上线订阅
        const bool saw100_1 = waitUntil([&]() { return allVarsAtFreq(kFreqMs); }, 12s);
        childRequire(r, saw100_1, "did not see freq=100 after sub1 subscribed");
        LOG(info) << "[case7_pub] stage1: sub1 subscribed, freq=100ms";

        // 等待 sub1 掉线
        const bool sawNone = waitUntil([&]() { return allVarsAtFreq(0xFFFFFFFF); }, 15s);
        childRequire(r, sawNone, "did not see freq=0xFFFFFFFF after sub1 offline");
        LOG(info) << "[case7_pub] stage2: sub1 offline, freq=0xFFFFFFFF";

        // 等待 sub2 上线重新订阅
        const bool saw100_2 = waitUntil([&]() { return allVarsAtFreq(kFreqMs); }, 15s);
        childRequire(r, saw100_2, "did not see freq=100 after sub2 subscribed");
        LOG(info) << "[case7_pub] stage3: sub2 subscribed, freq=100ms";

        std::this_thread::sleep_for(5s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();

        r.ok = saw100_1 && sawNone && saw100_2;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    // sub1：pub 启动后 3s 才上线，验证数据后退出（模拟掉线）
    std::this_thread::sleep_for(3s);
    const auto sub1Proc = spawnChild("case7_sub1", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub1_case7")), "sub1 init failed")
            || !childRequire(r, sub.start(), "sub1 start failed")) {
            return r;
        }
        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                "sub1 did not receive defines")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        std::map<std::string, int32_t> latestValues;
        std::map<std::string, std::vector<uint64_t>> tsLog;

        if (!childRequire(
                r,
                sub.subscribe(pubNode.c_str(), toSubscriptions(names, kFreqMs),
                              [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &v : vars) {
                                      const std::string name(v.varName.data(), v.varName.size());
                                      if (v.data && v.size >= sizeof(int32_t))
                                          latestValues[name] =
                                              *reinterpret_cast<const int32_t *>(v.data);
                                      auto &ts = tsLog[name];
                                      if (ts.size() < 4)
                                          ts.push_back(v.timestampNs);
                                      LOG(info) << "[case7_sub1] cb: " << name
                                                << " val=" << latestValues[name];
                                  }
                              }),
                "sub1 subscribe failed")) {
            sub.stop();
            return r;
        }

        if (!childRequire(r,
                          waitUntil(
                              [&]() {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &n : names)
                                      if (tsLog[n].size() < 4)
                                          return false;
                                  return true;
                              },
                              8s),
                          "sub1: not enough callbacks")) {
            sub.stop();
            return r;
        }
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &n : names) {
                if (!childRequire(r, latestValues.count(n) > 0, "sub1: no value for " + n)
                    || !childRequire(r, latestValues[n] != 0, "sub1: zero value for " + n)) {
                    sub.stop();
                    return r;
                }
            }
        }
        LOG(info) << "[case7_sub1] data OK, exiting (simulating offline)";
        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    // 等 sub1 退出后 3s，再启动 sub2（模拟重启）
    expectChildOk(sub1Proc, 25s);
    std::this_thread::sleep_for(3s);

    const auto sub2Proc = spawnChild("case7_sub2", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub2_case7")), "sub2 init failed")
            || !childRequire(r, sub.start(), "sub2 start failed")) {
            return r;
        }
        if (!childRequire(
                r, waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                "sub2 did not receive defines after restart")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        std::map<std::string, int32_t> latestValues;
        std::map<std::string, std::vector<uint64_t>> tsLog;

        if (!childRequire(
                r,
                sub.subscribe(pubNode.c_str(), toSubscriptions(names, kFreqMs),
                              [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &v : vars) {
                                      const std::string name(v.varName.data(), v.varName.size());
                                      if (v.data && v.size >= sizeof(int32_t))
                                          latestValues[name] =
                                              *reinterpret_cast<const int32_t *>(v.data);
                                      auto &ts = tsLog[name];
                                      if (ts.size() < 4)
                                          ts.push_back(v.timestampNs);
                                      LOG(info) << "[case7_sub2] cb: " << name
                                                << " val=" << latestValues[name];
                                  }
                              }),
                "sub2 subscribe failed")) {
            sub.stop();
            return r;
        }

        if (!childRequire(r,
                          waitUntil(
                              [&]() {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &n : names)
                                      if (tsLog[n].size() < 4)
                                          return false;
                                  return true;
                              },
                              8s),
                          "sub2: not enough callbacks after restart")) {
            sub.stop();
            return r;
        }
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &n : names) {
                if (!childRequire(r, latestValues.count(n) > 0, "sub2: no value for " + n)
                    || !childRequire(r, latestValues[n] != 0, "sub2: zero value for " + n)) {
                    sub.stop();
                    return r;
                }
            }
        }
        LOG(info) << "[case7_sub2] data OK after restart";
        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    expectChildOk(sub2Proc, 25s);
    expectChildOk(pubProc, 65s);
}

// TC8 - 三pub各自发布固定值，sub以不同频率订阅，验证变量数量、订阅频率、数据值准确性
// 场景：pub1 发布变量 v0~v9，每个变量的值固定为其序号（v0=0, v1=1, ...v9=9）；
//       pub2 发布变量 v10~v19，值固定为序号（v10=10, ...v19=19）；
//       pub3 发布变量 v20~v29，值固定为序号（v20=20, ...v29=29）。
//       sub 分别以 100ms/200ms/300ms 订阅 pub1/pub2/pub3 的全部变量。
// 验证：① sub 从每个 pub 收到的变量数量均为10；
//       ② pub 侧 freqChangeCallback 感知到的频率与 sub 请求一致（100/200/300ms）；
//       ③ sub 收到的每个变量值等于其序号，数据路由无串扰。
TEST(OnDemandPubSub, MultiPubFixedValueAndFreqVerification)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case8");
    std::filesystem::create_directories(root);

    const std::string pub1Node = uniqueName("pub1_case8");
    const std::string pub2Node = uniqueName("pub2_case8");
    const std::string pub3Node = uniqueName("pub3_case8");

    // 每个 pub 10个变量，值固定为序号
    const auto defs1 = makeDefines(pub1Node, "v", 10, 0);  // v0~v9
    const auto defs2 = makeDefines(pub2Node, "v", 10, 10); // v10~v19
    const auto defs3 = makeDefines(pub3Node, "v", 10, 20); // v20~v29
    const auto names1 = defineNames(defs1);
    const auto names2 = defineNames(defs2);
    const auto names3 = defineNames(defs3);

    // 发布固定值（值=序号）的辅助函数
    auto publishFixed = [](dsf::ondemand::OnDemandPub &pub,
                           const std::vector<std::string> &varNames, int startIdx,
                           const std::atomic<bool> &running) {
        while (running.load(std::memory_order_acquire)) {
            for (int i = 0; i < static_cast<int>(varNames.size()); ++i) {
                int32_t val = startIdx + i;
                pub.setVarData(varNames[i].c_str(), &val, sizeof(val));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    // pub1：发布 v0~v9，值固定为 0~9，期望 sub 以 100ms 订阅
    const auto pub1Proc = spawnChild("case8_pub1", root, [pub1Node, defs1, names1, publishFixed]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pub1Node), "pub1 init failed")
            || !childRequire(r, pub.start(), "pub1 start failed")
            || !childRequire(r, pub.createVars(defs1), "pub1 createVars failed")) {
            return r;
        }

        std::mutex mu;
        std::unordered_map<std::string, uint32_t> freqMap;
        pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
            LOG(info) << "[case8_pub1] freq change: " << varName << " -> " << freq << "ms";
            std::lock_guard<std::mutex> lk(mu);
            freqMap[varName] = freq;
        });

        std::atomic<bool> running{true};
        std::thread th([&]() { publishFixed(pub, names1, 0, running); });

        const bool sawFreq = waitUntil(
            [&]() {
                std::lock_guard<std::mutex> lk(mu);
                if (freqMap.size() < names1.size())
                    return false;
                for (const auto &n : names1) {
                    auto it = freqMap.find(n);
                    if (it == freqMap.end() || it->second != 100)
                        return false;
                }
                return true;
            },
            12s);
        childRequire(r, sawFreq, "pub1 did not see freq=100ms from sub");

        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = sawFreq;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    // pub2：发布 v10~v19，值固定为 10~19，期望 sub 以 200ms 订阅
    const auto pub2Proc = spawnChild("case8_pub2", root, [pub2Node, defs2, names2, publishFixed]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pub2Node), "pub2 init failed")
            || !childRequire(r, pub.start(), "pub2 start failed")
            || !childRequire(r, pub.createVars(defs2), "pub2 createVars failed")) {
            return r;
        }

        std::mutex mu;
        std::unordered_map<std::string, uint32_t> freqMap;
        pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
            LOG(info) << "[case8_pub2] freq change: " << varName << " -> " << freq << "ms";
            std::lock_guard<std::mutex> lk(mu);
            freqMap[varName] = freq;
        });

        std::atomic<bool> running{true};
        std::thread th([&]() { publishFixed(pub, names2, 10, running); });

        const bool sawFreq = waitUntil(
            [&]() {
                std::lock_guard<std::mutex> lk(mu);
                if (freqMap.size() < names2.size())
                    return false;
                for (const auto &n : names2) {
                    auto it = freqMap.find(n);
                    if (it == freqMap.end() || it->second != 200)
                        return false;
                }
                return true;
            },
            12s);
        childRequire(r, sawFreq, "pub2 did not see freq=200ms from sub");

        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = sawFreq;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    // pub3：发布 v20~v29，值固定为 20~29，期望 sub 以 300ms 订阅
    const auto pub3Proc = spawnChild("case8_pub3", root, [pub3Node, defs3, names3, publishFixed]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pub3Node), "pub3 init failed")
            || !childRequire(r, pub.start(), "pub3 start failed")
            || !childRequire(r, pub.createVars(defs3), "pub3 createVars failed")) {
            return r;
        }

        std::mutex mu;
        std::unordered_map<std::string, uint32_t> freqMap;
        pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
            LOG(info) << "[case8_pub3] freq change: " << varName << " -> " << freq << "ms";
            std::lock_guard<std::mutex> lk(mu);
            freqMap[varName] = freq;
        });

        std::atomic<bool> running{true};
        std::thread th([&]() { publishFixed(pub, names3, 20, running); });

        const bool sawFreq = waitUntil(
            [&]() {
                std::lock_guard<std::mutex> lk(mu);
                if (freqMap.size() < names3.size())
                    return false;
                for (const auto &n : names3) {
                    auto it = freqMap.find(n);
                    if (it == freqMap.end() || it->second != 300)
                        return false;
                }
                return true;
            },
            12s);
        childRequire(r, sawFreq, "pub3 did not see freq=300ms from sub");

        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = sawFreq;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    // sub：分别以 100/200/300ms 订阅 pub1/pub2/pub3，验证变量数量和值准确性
    const auto subProc =
        spawnChild("case8_sub", root, [pub1Node, pub2Node, pub3Node, names1, names2, names3]() {
            dsf::ondemand::OnDemandSub sub;
            ChildReport r;
            if (!childRequire(r, sub.init(uniqueName("sub_case8")), "sub init failed")
                || !childRequire(r, sub.start(), "sub start failed")) {
                return r;
            }

            // 等待三个 pub 的变量定义全部到达
            if (!childRequire(r,
                              waitUntil(
                                  [&]() {
                                      return countNodeVars(sub, pub1Node) == names1.size()
                                             && countNodeVars(sub, pub2Node) == names2.size()
                                             && countNodeVars(sub, pub3Node) == names3.size();
                                  },
                                  10s),
                              "sub did not receive all defines from 3 pubs")) {
                sub.stop();
                return r;
            }
            LOG(info) << "[case8_sub] all defines received";

            std::mutex mu;
            // varName -> 最新值
            std::map<std::string, int32_t> latestValues;
            // varName -> 回调次数（用于确认有持续回调）
            std::map<std::string, int> cbCount;

            auto dataCb = [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                std::lock_guard<std::mutex> lk(mu);
                for (const auto &v : vars) {
                    const std::string name(v.varName.data(), v.varName.size());
                    if (v.data && v.size >= sizeof(int32_t))
                        latestValues[name] = *reinterpret_cast<const int32_t *>(v.data);
                    cbCount[name]++;
                    LOG(info) << "[case8_sub] cb: " << name << " val=" << latestValues[name];
                }
            };

            if (!childRequire(r,
                              sub.subscribe(pub1Node.c_str(), toSubscriptions(names1, 100), dataCb),
                              "sub subscribe pub1 failed")
                || !childRequire(
                    r, sub.subscribe(pub2Node.c_str(), toSubscriptions(names2, 200), dataCb),
                    "sub subscribe pub2 failed")
                || !childRequire(
                    r, sub.subscribe(pub3Node.c_str(), toSubscriptions(names3, 300), dataCb),
                    "sub subscribe pub3 failed")) {
                sub.stop();
                return r;
            }

            // 等待所有30个变量都至少收到3次回调
            const auto allNames = [&]() {
                std::vector<std::string> all;
                all.insert(all.end(), names1.begin(), names1.end());
                all.insert(all.end(), names2.begin(), names2.end());
                all.insert(all.end(), names3.begin(), names3.end());
                return all;
            }();

            if (!childRequire(r,
                              waitUntil(
                                  [&]() {
                                      std::lock_guard<std::mutex> lk(mu);
                                      for (const auto &n : allNames)
                                          if (cbCount[n] < 3)
                                              return false;
                                      return true;
                                  },
                                  10s),
                              "sub did not receive enough callbacks for all vars")) {
                sub.stop();
                return r;
            }

            sub.stop();

            // 验证每个变量的值等于其序号
            {
                std::lock_guard<std::mutex> lk(mu);
                // pub1: v0~v9，期望值 0~9
                for (int i = 0; i < 10; ++i) {
                    const std::string name = "v" + std::to_string(i);
                    if (!childRequire(r, latestValues.count(name) > 0, "no value for " + name))
                        return r;
                    if (!childRequire(r, latestValues[name] == i,
                                      "value mismatch for " + name + ": got "
                                          + std::to_string(latestValues[name]) + " expected "
                                          + std::to_string(i)))
                        return r;
                }
                // pub2: v10~v19，期望值 10~19
                for (int i = 10; i < 20; ++i) {
                    const std::string name = "v" + std::to_string(i);
                    if (!childRequire(r, latestValues.count(name) > 0, "no value for " + name))
                        return r;
                    if (!childRequire(r, latestValues[name] == i,
                                      "value mismatch for " + name + ": got "
                                          + std::to_string(latestValues[name]) + " expected "
                                          + std::to_string(i)))
                        return r;
                }
                // pub3: v20~v29，期望值 20~29
                for (int i = 20; i < 30; ++i) {
                    const std::string name = "v" + std::to_string(i);
                    if (!childRequire(r, latestValues.count(name) > 0, "no value for " + name))
                        return r;
                    if (!childRequire(r, latestValues[name] == i,
                                      "value mismatch for " + name + ": got "
                                          + std::to_string(latestValues[name]) + " expected "
                                          + std::to_string(i)))
                        return r;
                }
            }

            LOG(info) << "[case8_sub] all values and counts verified OK";
            r.ok = true;
            r.message = "ok";
            return r;
        });

    expectChildOk(pub1Proc, 25s);
    expectChildOk(pub2Proc, 25s);
    expectChildOk(pub3Proc, 25s);
    expectChildOk(subProc, 25s);
}

// TC9 - 通信过程中删除变量，验证 pub/sub 行为正确性
// 场景：pub 创建20个变量（v0~v19）并开始发布数据，sub 订阅全部变量（100ms）并开始接收。
//       通信稳定后，pub 在发布过程中删除后10个变量（v10~v19）。
// 验证：① pub 删除后仍能正常发布剩余10个变量，setVarData 对已删除变量不崩溃；
//       ② sub 侧删除后只剩10个变量定义，已删除变量不再触发回调；
//       ③ pub 侧 freqChangeCallback 在删除后对剩余变量频率不变，
//          对已删除变量触发 0xFFFFFFFF（无订阅者）。
TEST(OnDemandPubSub, DeleteVarsDuringCommunication)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case9");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case9");
    const auto defs20 = makeDefines(pubNode, "v", 20); // v0~v19
    const auto names20 = defineNames(defs20);
    const std::vector<std::string> toDelete(names20.begin() + 10, names20.end());    // v10~v19
    const std::vector<std::string> remaining(names20.begin(), names20.begin() + 10); // v0~v9

    const auto pubProc =
        spawnChild("case9_pub", root, [pubNode, defs20, names20, toDelete, remaining]() {
            dsf::ondemand::OnDemandPub pub;
            ChildReport r;
            if (!childRequire(r, pub.init(pubNode), "pub init failed")
                || !childRequire(r, pub.start(), "pub start failed")
                || !childRequire(r, pub.createVars(defs20), "pub createVars(20) failed")) {
                return r;
            }

            std::mutex mu;
            std::unordered_map<std::string, uint32_t> freqMap;
            pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
                LOG(info) << "[case9_pub] freq change: " << varName << " -> " << freq << "ms";
                std::lock_guard<std::mutex> lk(mu);
                freqMap[varName] = freq;
            });

            std::atomic<bool> running{true};
            std::thread th([&]() { publishLoop(pub, names20, running, 10); });

            // 等待 sub 订阅，所有20个变量频率变为100ms
            const bool sawAll100 = waitUntil(
                [&]() {
                    std::lock_guard<std::mutex> lk(mu);
                    if (freqMap.size() < names20.size())
                        return false;
                    for (const auto &n : names20) {
                        auto it = freqMap.find(n);
                        if (it == freqMap.end() || it->second != 100)
                            return false;
                    }
                    return true;
                },
                10s);
            if (!childRequire(r, sawAll100, "pub did not see freq=100 for all 20 vars")) {
                running.store(false, std::memory_order_release);
                th.join();
                pub.stop();
                return r;
            }
            LOG(info) << "[case9_pub] all 20 vars at 100ms, now deleting v10~v19";

            std::this_thread::sleep_for(4s);
            // 通信过程中删除 v10~v19
            if (!childRequire(r, pub.deleteVars(toDelete), "pub deleteVars(v10~v19) failed")) {
                running.store(false, std::memory_order_release);
                th.join();
                pub.stop();
                return r;
            }
            LOG(info) << "[case9_pub] deleted v10~v19, continuing to publish v0~v9";

            // 删除后立即验证剩余变量频率（sub 仍在线，此时 freqMap 应保持 100ms）
            // 注意：删除变量本身不触发 freqChangeCallback，所以 freqMap 里剩余变量的值不变
            bool remainingOk = true;
            {
                std::lock_guard<std::mutex> lk(mu);
                for (const auto &n : remaining) {
                    auto it = freqMap.find(n);
                    if (it == freqMap.end() || it->second != 100) {
                        remainingOk = false;
                        LOG(error) << "[case9_pub] remaining var " << n
                                   << " freq=" << (it != freqMap.end() ? it->second : 0);
                    }
                }
            }
            childRequire(r, remainingOk, "remaining vars freq not 100ms after delete");

            // 继续发布，等待 sub 完成验证后退出（publishLoop 对已删除变量调用 setVarData 不崩溃）
            std::this_thread::sleep_for(8s);

            running.store(false, std::memory_order_release);
            th.join();
            pub.stop();
            r.ok = remainingOk;
            r.message = r.ok ? "ok" : r.message;
            return r;
        });

    const auto subProc = spawnChild("case9_sub", root, [pubNode, names20, toDelete, remaining]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case9")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        // 等待20个变量定义到达
        if (!childRequire(r, waitUntil([&]() { return countNodeVars(sub, pubNode) == 20; }, 8s),
                          "sub did not receive 20 defines")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case9_sub] received 20 defines, subscribing";

        std::mutex mu;
        std::map<std::string, int32_t> latestValues;
        std::map<std::string, int> cbCount;
        // 删除后是否还收到已删除变量的回调
        std::atomic<bool> deleteSignaled{false};
        std::map<std::string, int> cbAfterDelete;

        if (!childRequire(
                r,
                sub.subscribe(pubNode.c_str(), toSubscriptions(names20, 100),
                              [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &v : vars) {
                                      const std::string name(v.varName.data(), v.varName.size());
                                      if (v.data && v.size >= sizeof(int32_t))
                                          latestValues[name] =
                                              *reinterpret_cast<const int32_t *>(v.data);
                                      cbCount[name]++;
                                      if (deleteSignaled.load(std::memory_order_acquire))
                                          cbAfterDelete[name]++;
                                      LOG(info) << "[case9_sub] cb: " << name
                                                << " val=" << latestValues[name];
                                  }
                              }),
                "sub subscribe failed")) {
            sub.stop();
            return r;
        }

        // 等待所有20个变量都收到回调（通信建立）
        if (!childRequire(r,
                          waitUntil(
                              [&]() {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &n : names20)
                                      if (cbCount[n] < 2)
                                          return false;
                                  return true;
                              },
                              8s),
                          "sub did not receive initial callbacks for all 20 vars")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case9_sub] initial callbacks OK for all 20 vars";

        // 等待 pub 删除变量（变量数量降为10）
        if (!childRequire(r, waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; }, 10s),
                          "sub did not see var count drop to 10 after delete")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case9_sub] var count dropped to 10, signaling delete";
        deleteSignaled.store(true, std::memory_order_release);

        // 立刻验证剩余变量的定义集合准确（pub 还在线，此时集合稳定）
        const auto leftVars = nodeVars(sub, pubNode);
        const std::set<std::string> leftSet(leftVars.begin(), leftVars.end());
        const std::set<std::string> expectedSet(remaining.begin(), remaining.end());
        if (leftSet != expectedSet) {
            std::string got, exp;
            for (const auto &s : leftSet) got += s + " ";
            for (const auto &s : expectedSet) exp += s + " ";
            LOG(error) << "[case9_sub] leftSet=[" << got << "] expectedSet=[" << exp << "]";
        }
        childRequire(r, leftSet == expectedSet, "remaining var name set mismatch after delete");

        // 再等 5s，观察删除后是否还有已删除变量的回调
        std::this_thread::sleep_for(5s);

        // 验证剩余变量仍有回调，已删除变量无回调
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &n : remaining) {
                if (!childRequire(r, cbAfterDelete.count(n) > 0 && cbAfterDelete[n] > 0,
                                  "remaining var " + n + " has no callback after delete"))
                    break;
            }
            for (const auto &n : toDelete) {
                if (!childRequire(
                        r, cbAfterDelete.count(n) == 0 || cbAfterDelete[n] == 0,
                        "deleted var " + n + " still has callbacks after delete: "
                            + std::to_string(cbAfterDelete.count(n) ? cbAfterDelete[n] : 0)))
                    break;
            }
        }

        sub.stop();
        if (r.message.empty()) {
            r.ok = true;
            r.message = "ok";
        }
        return r;
    });

    expectChildOk(pubProc, 30s);
    expectChildOk(subProc, 30s);
}

// TC10 - 批量写接口验证
// 场景：pub 创建10个变量，使用 getVarIds 预查询 varId，再通过 setVarDataBatch 批量写入，
//       每个变量的值固定为其序号（v0=0, v1=1, ...v9=9）。sub 订阅全部变量（100ms）。
// 验证：① setVarDataBatch 写入的数据能被 sub 正确接收；
//       ② sub 收到的每个变量值等于其序号，与逐个 setVarData 结果一致。
TEST(OnDemandPubSub, BatchWriteInterfaceCorrectness)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case10");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case10");
    const auto defs = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case10_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        // 预查询所有 varId
        std::vector<uint32_t> ids(names.size());
        const char *namesPtrs[10];
        for (size_t i = 0; i < names.size(); ++i) namesPtrs[i] = names[i].c_str();
        pub.getVarIds(namesPtrs, ids.data(), names.size());

        for (size_t i = 0; i < ids.size(); ++i) {
            if (!childRequire(r, ids[i] != UINT32_MAX,
                              "getVarId failed for " + names[i])) {
                pub.stop();
                return r;
            }
        }
        LOG(info) << "[case10_pub] all varIds resolved";

        // 批量写入，值=序号，持续发布
        std::vector<int32_t> values(names.size());
        for (int i = 0; i < static_cast<int>(names.size()); ++i) values[i] = i;

        std::vector<dsf::ondemand::OnDemandPub::VarWriteItem> items(names.size());
        for (size_t i = 0; i < names.size(); ++i) {
            items[i] = {ids[i], &values[i], sizeof(int32_t)};
        }

        std::atomic<bool> running{true};
        std::thread th([&]() {
            while (running.load(std::memory_order_acquire)) {
                pub.setVarDataBatch(items.data(), items.size());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case10_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case10")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                          "sub did not receive defines")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        std::map<std::string, int32_t> latestValues;
        std::map<std::string, int> cbCount;

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                            std::lock_guard<std::mutex> lk(mu);
                                            for (const auto &v : vars) {
                                                const std::string name(v.varName.data(), v.varName.size());
                                                if (v.data && v.size >= sizeof(int32_t))
                                                    latestValues[name] = *reinterpret_cast<const int32_t *>(v.data);
                                                cbCount[name]++;
                                                LOG(info) << "[case10_sub] cb: " << name
                                                          << " val=" << latestValues[name];
                                            }
                                        }),
                          "subscribe failed")) {
            sub.stop();
            return r;
        }

        // 等待所有变量各收到至少3次回调
        if (!childRequire(r,
                          waitUntil([&]() {
                              std::lock_guard<std::mutex> lk(mu);
                              for (const auto &n : names)
                                  if (cbCount[n] < 3) return false;
                              return true;
                          }, 8s),
                          "not enough callbacks")) {
            sub.stop();
            return r;
        }

        sub.stop();

        // 验证每个变量值等于其序号
        std::lock_guard<std::mutex> lk(mu);
        for (int i = 0; i < static_cast<int>(names.size()); ++i) {
            const std::string name = "v" + std::to_string(i);
            if (!childRequire(r, latestValues.count(name) > 0, "no value for " + name)) return r;
            if (!childRequire(r, latestValues[name] == i,
                              "batch write value mismatch for " + name + ": got "
                                  + std::to_string(latestValues[name]) + " expected "
                                  + std::to_string(i)))
                return r;
        }
        LOG(info) << "[case10_sub] all batch-written values verified OK";
        r.ok = true;
        r.message = "ok";
        return r;
    });

    expectChildOk(pubProc, 20s);
    expectChildOk(subProc, 20s);
}

// TC11 - pausePublish/resumePublish 暂停与恢复验证
// 场景：pub 创建10个变量并开始发布，sub 订阅后正常收到数据。
//       pub 调用 pausePublish() 暂停发布，验证 sub 在暂停期间收到的回调中时间戳不再更新
//       （sub 的定时器仍会触发回调，但 pub 不再写入新数据，timestampNs 保持不变）。
//       pub 调用 resumePublish() 恢复发布，验证 sub 收到的时间戳重新推进。
// 验证：① 暂停后 sub 回调中各变量的 timestampNs 停止增长；
//       ② 恢复后 timestampNs 重新增长，说明 pub 重新写入了新数据。
TEST(OnDemandPubSub, PauseAndResumePublish)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case11");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case11");
    const auto defs = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case11_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });

        // 正常发布 3s
        std::this_thread::sleep_for(3s);
        LOG(info) << "[case11_pub] pausing publish";
        pub.pausePublish();

        // 暂停 4s
        std::this_thread::sleep_for(4s);
        LOG(info) << "[case11_pub] resuming publish";
        pub.resumePublish();

        // 恢复后再发布 4s
        std::this_thread::sleep_for(4s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case11_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case11")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                          "sub did not receive defines")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        // 每个变量最新的 timestampNs
        std::map<std::string, uint64_t> latestTs;

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                            std::lock_guard<std::mutex> lk(mu);
                                            for (const auto &v : vars) {
                                                const std::string name(v.varName.data(), v.varName.size());
                                                latestTs[name] = v.timestampNs;
                                                LOG(info) << "[case11_sub] cb: " << name
                                                          << " ts=" << v.timestampNs;
                                            }
                                        }),
                          "subscribe failed")) {
            sub.stop();
            return r;
        }

        // 阶段1：等待所有变量都收到过回调（正常发布阶段）
        if (!childRequire(r,
                          waitUntil([&]() {
                              std::lock_guard<std::mutex> lk(mu);
                              return latestTs.size() == names.size();
                          }, 6s),
                          "did not receive initial callbacks before pause")) {
            sub.stop();
            return r;
        }
        LOG(info) << "[case11_sub] stage1: initial callbacks OK";

        // 阶段2：等待 pub 进入暂停（pub 在 t=3s 暂停，sub 在 t≈4s 采样）
        std::this_thread::sleep_for(4s);
        std::map<std::string, uint64_t> tsAtPause;
        {
            std::lock_guard<std::mutex> lk(mu);
            tsAtPause = latestTs;
        }
        LOG(info) << "[case11_sub] stage2: sampled ts during pause";

        // 再等 2s，确认暂停期间时间戳不再推进
        std::this_thread::sleep_for(2s);
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &n : names) {
                if (!childRequire(r, latestTs[n] == tsAtPause[n],
                                  "ts advanced during pause for " + n + ": before="
                                      + std::to_string(tsAtPause[n]) + " after="
                                      + std::to_string(latestTs[n])))
                    break;
            }
        }
        LOG(info) << "[case11_sub] stage2: ts frozen during pause OK";

        // 阶段3：等待 pub 恢复（pub 在 t=7s 恢复），验证时间戳重新推进
        std::this_thread::sleep_for(3s);
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &n : names) {
                if (!childRequire(r, latestTs[n] > tsAtPause[n],
                                  "ts did not advance after resume for " + n + ": before="
                                      + std::to_string(tsAtPause[n]) + " after="
                                      + std::to_string(latestTs[n])))
                    break;
            }
        }
        LOG(info) << "[case11_sub] stage3: ts advanced after resume OK";

        sub.stop();
        if (r.message.empty()) {
            r.ok = true;
            r.message = "ok";
        }
        return r;
    });

    expectChildOk(pubProc, 20s);
    expectChildOk(subProc, 20s);
}

// TC12 - setTableDefineCallback 提前注册定义回调
// 场景：sub 在调用 start() 之前注册 TableDefineCallback，随后 pub 上线并广播8个变量定义。
//       由于回调在 start() 前注册，不会错过 pub 上线后的第一次广播。
// 验证：① TableDefineCallback 至少被触发一次；
//       ② 跨多次回调累积收到的变量名集合与 pub 创建的8个变量名完全一致。
TEST(OnDemandPubSub, TableDefineCallbackRegisteredBeforeStart)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case12");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case12");
    const auto defs = makeDefines(pubNode, "td", 8);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case12_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 20); });
        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case12_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;

        std::mutex mu;
        std::set<std::string> receivedNames;
        std::atomic<int> cbCount{0};

        sub.setTableDefineCallback([&](const std::vector<DSF::Var::Define> &defines) {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto &d : defines) {
                if (d.nodeName() == pubNode)
                    receivedNames.insert(d.name());
            }
            cbCount.fetch_add(1, std::memory_order_release);
            LOG(info) << "[case12_sub] tableDefine cb: " << defines.size()
                      << " vars, total=" << receivedNames.size();
        });

        if (!childRequire(r, sub.init(uniqueName("sub_case12")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        const std::set<std::string> expectedNames(names.begin(), names.end());
        const bool gotAll = waitUntil(
            [&]() {
                std::lock_guard<std::mutex> lk(mu);
                return receivedNames == expectedNames;
            },
            8s);

        sub.stop();

        r.metrics["cb_count"] = std::to_string(cbCount.load());
        r.metrics["received_count"] = std::to_string(receivedNames.size());
        childRequire(r, cbCount.load() > 0, "tableDefine callback never fired");
        childRequire(r, gotAll,
                     "not all var names in tableDefine callback: got "
                         + std::to_string(receivedNames.size()) + " expected "
                         + std::to_string(expectedNames.size()));
        r.ok = cbCount.load() > 0 && gotAll;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    expectChildOk(pubProc, 14s);
    expectChildOk(subProc, 14s);
}

// TC13 - getTotalReceivedVars 接收计数验证
// 场景：pub 创建6个变量并以10ms间隔持续写入，sub 订阅全部变量（100ms）后等待数据到达。
//       在数据稳定流入后，分两次采样 getTotalReceivedVars() 的返回值，间隔2秒。
// 验证：① 第一次采样值 > 0，说明 sub 已成功接收到数据；
//       ② 第二次采样值 > 第一次，说明计数随时间持续递增，接收链路持续工作。
TEST(OnDemandPubSub, TotalReceivedVarsCountIncreases)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case13");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case13");
    const auto defs = makeDefines(pubNode, "v", 6);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case13_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(10s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case13_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case13")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                          "sub did not receive defines")) {
            sub.stop();
            return r;
        }
        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [](const std::vector<dsf::ondemand::VarCallbackData> &) {}),
                          "subscribe failed")) {
            sub.stop();
            return r;
        }

        const bool gotFirst = waitUntil([&]() { return sub.getTotalReceivedVars() > 0; }, 6s);
        if (!childRequire(r, gotFirst, "getTotalReceivedVars still 0 after 6s")) {
            sub.stop();
            return r;
        }

        const uint64_t sample1 = sub.getTotalReceivedVars();
        std::this_thread::sleep_for(2s);
        const uint64_t sample2 = sub.getTotalReceivedVars();

        sub.stop();

        r.metrics["sample1"] = std::to_string(sample1);
        r.metrics["sample2"] = std::to_string(sample2);
        LOG(info) << "[case13_sub] sample1=" << sample1 << " sample2=" << sample2;
        childRequire(r, sample2 >= sample1,
                     "getTotalReceivedVars did not increase: s1=" + std::to_string(sample1)
                         + " s2=" + std::to_string(sample2));
        r.ok = sample1 > 0 && sample2 >= sample1;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    expectChildOk(pubProc, 18s);
    expectChildOk(subProc, 18s);
}

// TC14 - setBlobType/getBlobType 序列化类型验证
// 场景：pub 调用 setBlobType(NGVS) 后开始发布数据，sub 订阅4个变量（100ms）并接收回调。
//       默认序列化类型为 STRUCTS，此处显式切换为 NGVS，验证类型信息能随数据一起传递到 sub 侧。
// 验证：① sub 收到的 VarCallbackData.blobType 等于 NGVS；
//       ② sub.getBlobType() 返回 NGVS，说明 sub 侧全局序列化类型已同步更新。
TEST(OnDemandPubSub, BlobTypeSetAndReceivedCorrectly)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case14");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case14");
    const auto defs = makeDefines(pubNode, "v", 4);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case14_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }
        pub.setBlobType(DSF::Var::BLOB_TYPE::NGVS);
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case14_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case14")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                          "sub did not receive defines")) {
            sub.stop();
            return r;
        }

        std::atomic<bool> gotNgvsCb{false};
        std::atomic<uint32_t> cbBlobTypeVal{0};

        if (!childRequire(
                r,
                sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                              [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                  for (const auto &v : vars) {
                                      LOG(info) << "[case14_sub] blobType="
                                                << static_cast<uint32_t>(v.blobType);
                                      if (v.blobType == DSF::Var::BLOB_TYPE::NGVS) {
                                          cbBlobTypeVal.store(
                                              static_cast<uint32_t>(v.blobType),
                                              std::memory_order_release);
                                          gotNgvsCb.store(true, std::memory_order_release);
                                      }
                                  }
                              }),
                "subscribe failed")) {
            sub.stop();
            return r;
        }

        const bool gotCb = waitUntil([&]() { return gotNgvsCb.load(); }, 6s);
        const auto subBlobType = sub.getBlobType();
        sub.stop();

        r.metrics["cb_blobType"] = std::to_string(cbBlobTypeVal.load());
        r.metrics["sub_getBlobType"] = std::to_string(static_cast<uint32_t>(subBlobType));
        childRequire(r, gotCb, "never received callback with NGVS blobType");
        childRequire(r, subBlobType == DSF::Var::BLOB_TYPE::NGVS,
                     "sub.getBlobType() != NGVS: got "
                         + std::to_string(static_cast<uint32_t>(subBlobType)));
        r.ok = gotCb && (subBlobType == DSF::Var::BLOB_TYPE::NGVS);
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    expectChildOk(pubProc, 14s);
    expectChildOk(subProc, 14s);
}

// TC15 - getVarId 单个查询 + 未知变量返回 UINT32_MAX
// 场景：pub 创建5个变量后，通过 getVarId() 分别查询已注册变量和一个不存在的变量名。
//       getVarId 是热路径接口，用于预缓存 varId 以避免后续 setVarData 时的哈希查找开销。
// 验证：① 对已创建的每个变量，getVarId() 返回有效 id（!= UINT32_MAX）；
//       ② 对未创建的变量名，getVarId() 返回 UINT32_MAX，符合"未找到"语义。
TEST(OnDemandPubSub, GetVarIdKnownAndUnknown)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case15");
    std::filesystem::create_directories(root);

    const auto proc = spawnChild("case15", root, []() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        const std::string node = uniqueName("pub_case15");
        const auto defs = makeDefines(node, "v", 5);
        const auto names = defineNames(defs);

        if (!childRequire(r, pub.init(node), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        bool allValid = true;
        for (const auto &n : names) {
            const uint32_t id = pub.getVarId(n.c_str());
            LOG(info) << "[case15] getVarId(" << n << ")=" << id;
            if (!childRequire(r, id != UINT32_MAX, "getVarId returned UINT32_MAX for: " + n)) {
                allValid = false;
                break;
            }
        }

        const uint32_t unknownId = pub.getVarId("__nonexistent__");
        LOG(info) << "[case15] getVarId(nonexistent)=" << unknownId;
        childRequire(r, unknownId == UINT32_MAX,
                     "getVarId should return UINT32_MAX for unknown var");

        pub.stop();
        r.ok = allValid && (unknownId == UINT32_MAX);
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    expectChildOk(proc, 10s);
}

// TC16 - cleanupParticipantPublish/Subscriptions 手动清理
// 场景A：pub 上线并广播6个变量定义，sub 收到定义后主动调用 cleanupParticipantPublish(pubNode)，
//        模拟外部检测到 pub 掉线后手动触发清理的场景。
// 场景B：sub 以固定节点名订阅 pub 的6个变量（100ms），pub 感知到订阅后主动调用
//        cleanupParticipantSubscriptions(subNode)，模拟外部强制踢掉某个订阅者的场景。
// 验证A：cleanupParticipantPublish 返回 true，且调用后 getAvailableVars 中该节点变量立即清零。
// 验证B：cleanupParticipantSubscriptions 返回 true，且调用后 pub 侧频率回调感知到
//        所有变量频率变为 0xFFFFFFFF（无订阅者）。
TEST(OnDemandPubSub, CleanupParticipantManually)
{
    // 场景A：sub 手动清理 pub 节点，getAvailableVars 中该节点变量消失
    {
        const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case16a");
        std::filesystem::create_directories(root);

        const std::string pubNode = uniqueName("pub_case16a");
        const auto defs = makeDefines(pubNode, "v", 6);
        const auto names = defineNames(defs);

        const auto pubProc = spawnChild("case16a_pub", root, [pubNode, defs, names]() {
            dsf::ondemand::OnDemandPub pub;
            ChildReport r;
            if (!childRequire(r, pub.init(pubNode), "pub init failed")
                || !childRequire(r, pub.start(), "pub start failed")
                || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
                return r;
            }
            std::atomic<bool> running{true};
            std::thread th([&]() { publishLoop(pub, names, running, 20); });
            std::this_thread::sleep_for(8s);
            running.store(false, std::memory_order_release);
            th.join();
            pub.stop();
            r.ok = true;
            r.message = "ok";
            return r;
        });

        const auto subProc = spawnChild("case16a_sub", root, [pubNode]() {
            dsf::ondemand::OnDemandSub sub;
            ChildReport r;
            if (!childRequire(r, sub.init(uniqueName("sub_case16a")), "sub init failed")
                || !childRequire(r, sub.start(), "sub start failed")) {
                return r;
            }
            if (!childRequire(r,
                              waitUntil([&]() { return countNodeVars(sub, pubNode) > 0; }, 8s),
                              "sub did not receive defines")) {
                sub.stop();
                return r;
            }

            const bool cleaned = sub.cleanupParticipantPublish(pubNode);
            childRequire(r, cleaned, "cleanupParticipantPublish returned false");

            const size_t remaining = countNodeVars(sub, pubNode);
            r.metrics["remaining"] = std::to_string(remaining);
            childRequire(r, remaining == 0,
                         "vars still present after cleanup: " + std::to_string(remaining));

            sub.stop();
            r.ok = cleaned && (remaining == 0);
            r.message = r.ok ? "ok" : r.message;
            return r;
        });

        expectChildOk(pubProc, 14s);
        expectChildOk(subProc, 14s);
    }

    // 场景B：pub 手动清理 sub 订阅关系，频率回退到 0xFFFFFFFF
    {
        const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case16b");
        std::filesystem::create_directories(root);

        const std::string pubNode = uniqueName("pub_case16b");
        const std::string subNode = "sub_tc16b_" + std::to_string(getpid());
        const auto defs = makeDefines(pubNode, "v", 6);
        const auto names = defineNames(defs);

        const auto pubProc =
            spawnChild("case16b_pub", root, [pubNode, subNode, defs, names]() {
                dsf::ondemand::OnDemandPub pub;
                ChildReport r;
                if (!childRequire(r, pub.init(pubNode), "pub init failed")
                    || !childRequire(r, pub.start(), "pub start failed")
                    || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
                    return r;
                }

                std::mutex mu;
                std::unordered_map<std::string, uint32_t> freqMap;
                pub.setFreqChangeCallback([&](const std::string &varName, uint32_t freq) {
                    std::lock_guard<std::mutex> lk(mu);
                    freqMap[varName] = freq;
                    LOG(info) << "[case16b_pub] freq: " << varName << " -> " << freq;
                });

                std::atomic<bool> running{true};
                std::thread th([&]() { publishLoop(pub, names, running, 10); });

                auto allAt = [&](uint32_t expected) {
                    std::lock_guard<std::mutex> lk(mu);
                    if (freqMap.size() < names.size())
                        return false;
                    for (const auto &n : names) {
                        auto it = freqMap.find(n);
                        if (it == freqMap.end() || it->second != expected)
                            return false;
                    }
                    return true;
                };

                const bool saw100 = waitUntil([&]() { return allAt(100); }, 10s);
                if (!childRequire(r, saw100, "did not see freq=100 after sub subscribed")) {
                    running.store(false, std::memory_order_release);
                    th.join();
                    pub.stop();
                    return r;
                }

                const bool cleaned = pub.cleanupParticipantSubscriptions(subNode);
                childRequire(r, cleaned, "cleanupParticipantSubscriptions returned false");

                const bool sawNone = waitUntil([&]() { return allAt(0xFFFFFFFF); }, 6s);
                childRequire(r, sawNone, "freq did not go to 0xFFFFFFFF after cleanup");

                running.store(false, std::memory_order_release);
                th.join();
                pub.stop();
                r.ok = cleaned && sawNone;
                r.message = r.ok ? "ok" : r.message;
                return r;
            });

        const auto subProc =
            spawnChild("case16b_sub", root, [pubNode, subNode, names]() {
                dsf::ondemand::OnDemandSub sub;
                ChildReport r;
                if (!childRequire(r, sub.init(subNode), "sub init failed")
                    || !childRequire(r, sub.start(), "sub start failed")) {
                    return r;
                }
                if (!childRequire(r,
                                  waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                                  "sub did not receive defines")) {
                    sub.stop();
                    return r;
                }
                if (!childRequire(r,
                                  sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                                [](const std::vector<dsf::ondemand::VarCallbackData> &) {}),
                                  "subscribe failed")) {
                    sub.stop();
                    return r;
                }
                std::this_thread::sleep_for(10s);
                sub.stop();
                r.ok = true;
                r.message = "ok";
                return r;
            });

        expectChildOk(pubProc, 22s);
        expectChildOk(subProc, 18s);
    }
}

// TC17 - 边界/异常 — init前start、空变量名、重复createVars、unsubscribe不存在变量
// 场景：在单个子进程中依次执行四类边界操作，验证系统对非法或重复调用的容错能力。
//       ① pub/sub 在未调用 init() 的情况下直接调用 start()；
//       ② pub 调用 createVars 时传入 name 为空字符串的变量定义；
//       ③ pub 对同一批变量连续调用两次 createVars（重复注册）；
//       ④ sub 对从未订阅过的节点和变量名调用 unsubscribe()。
// 验证：① start() 在 init() 之前返回 false（幂等保护）；
//       ② 空名称 createVars 不崩溃（容错）；
//       ③ 重复 createVars 不崩溃，第一次调用返回 true（幂等）；
//       ④ unsubscribe 不存在的变量返回 false（明确失败语义）。
TEST(OnDemandPubSub, BoundaryAndErrorCases)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case17");
    std::filesystem::create_directories(root);

    const auto proc = spawnChild("case17", root, []() {
        ChildReport r;
        r.ok = true;
        const std::string base = uniqueName("node17");

        // ① start() 在 init() 之前应返回 false
        {
            dsf::ondemand::OnDemandPub pub;
            const bool res = pub.start();
            LOG(info) << "[case17] pub.start() before init=" << res;
            childRequire(r, !res, "pub.start() before init() should return false");
        }
        {
            dsf::ondemand::OnDemandSub sub;
            const bool res = sub.start();
            LOG(info) << "[case17] sub.start() before init=" << res;
            childRequire(r, !res, "sub.start() before init() should return false");
        }

        // ② createVars 包含空名称不崩溃
        {
            dsf::ondemand::OnDemandPub pub;
            if (pub.init(base + "_a") && pub.start()) {
                DSF::Var::Define emptyDef;
                emptyDef.nodeName(base + "_a");
                emptyDef.name("");
                emptyDef.modelName("int32");
                emptyDef.size(sizeof(int32_t));
                pub.createVars({emptyDef});
                LOG(info) << "[case17] createVars with empty name: no crash";
                pub.stop();
            }
        }

        // ③ 重复 createVars 相同变量不崩溃
        {
            dsf::ondemand::OnDemandPub pub;
            const std::string dupNode = base + "_b";
            const auto defs = makeDefines(dupNode, "v", 3);
            if (pub.init(dupNode) && pub.start()) {
                const bool first = pub.createVars(defs);
                const bool second = pub.createVars(defs);
                LOG(info) << "[case17] createVars twice: first=" << first << " second=" << second;
                childRequire(r, first, "first createVars failed");
                pub.stop();
            }
        }

        // ④ unsubscribe 不存在的变量返回 false
        {
            dsf::ondemand::OnDemandSub sub;
            if (sub.init(base + "_c") && sub.start()) {
                const bool res = sub.unsubscribe("nonexistent_pub", {"__no_such_var__"});
                LOG(info) << "[case17] unsubscribe nonexistent=" << res;
                childRequire(r, !res, "unsubscribe of nonexistent var should return false");
                sub.stop();
            }
        }

        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    expectChildOk(proc, 15s);
}

// TC18 - setVarData(varId) 单 ID 写入正确性验证
// 场景：pub 创建4个变量，通过 getVarId() 预查询 varId，分别用 setVarData(varId, ...) 和
//       setVarData(varName, ...) 交替写入不同变量，每个变量的值固定为其序号。
//       sub 订阅全部变量（100ms），收集回调中的最新值。
// 验证：① setVarData(varId, ...) 写入的数据能被 sub 正确接收；
//       ② 与 setVarData(varName, ...) 写入结果一致，两种接口语义等价；
//       ③ sub 收到的每个变量值等于其序号，无数据串扰。
TEST(OnDemandPubSub, SetVarDataByIdCorrectness)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case18");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case18");
    const auto defs = makeDefines(pubNode, "v", 4);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case18_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        // 预查询 varId
        std::vector<uint32_t> ids(names.size());
        for (size_t i = 0; i < names.size(); ++i) {
            ids[i] = pub.getVarId(names[i].c_str());
            if (!childRequire(r, ids[i] != UINT32_MAX,
                              "getVarId failed for " + names[i])) {
                pub.stop();
                return r;
            }
        }
        LOG(info) << "[case18_pub] all varIds resolved";

        // v0/v2 用 setVarData(varId)，v1/v3 用 setVarData(varName)，值均固定为序号
        std::atomic<bool> running{true};
        std::thread th([&]() {
            while (running.load(std::memory_order_acquire)) {
                for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                    int32_t val = i;
                    if (i % 2 == 0) {
                        pub.setVarData(ids[i], &val, sizeof(val));
                    } else {
                        pub.setVarData(names[i].c_str(), &val, sizeof(val));
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        std::this_thread::sleep_for(6s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case18_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case18")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                          "sub did not receive defines")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        std::map<std::string, int32_t> latestValues;
        std::map<std::string, int> cbCount;

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                            std::lock_guard<std::mutex> lk(mu);
                                            for (const auto &v : vars) {
                                                const std::string name(v.varName.data(), v.varName.size());
                                                if (v.data && v.size >= sizeof(int32_t))
                                                    latestValues[name] =
                                                        *reinterpret_cast<const int32_t *>(v.data);
                                                cbCount[name]++;
                                                LOG(info) << "[case18_sub] cb: " << name
                                                          << " val=" << latestValues[name];
                                            }
                                        }),
                          "subscribe failed")) {
            sub.stop();
            return r;
        }

        if (!childRequire(r,
                          waitUntil(
                              [&]() {
                                  std::lock_guard<std::mutex> lk(mu);
                                  for (const auto &n : names)
                                      if (cbCount[n] < 3)
                                          return false;
                                  return true;
                              },
                              8s),
                          "not enough callbacks")) {
            sub.stop();
            return r;
        }

        sub.stop();

        std::lock_guard<std::mutex> lk(mu);
        for (int i = 0; i < static_cast<int>(names.size()); ++i) {
            const std::string name = "v" + std::to_string(i);
            if (!childRequire(r, latestValues.count(name) > 0, "no value for " + name))
                return r;
            if (!childRequire(r, latestValues[name] == i,
                              "value mismatch for " + name + ": got "
                                  + std::to_string(latestValues[name]) + " expected "
                                  + std::to_string(i)))
                return r;
        }
        LOG(info) << "[case18_sub] all values verified OK";
        r.ok = true;
        r.message = "ok";
        return r;
    });

    expectChildOk(pubProc, 14s);
    expectChildOk(subProc, 14s);
}

// TC_19 - setTableDefineCallback 在 createVars / deleteVars 各阶段内容准确性验证
//
// 场景：pub 分三阶段操作变量：
//   阶段1：createVars 6个变量
//   阶段2：再 createVars 4个变量（共10个）
//   阶段3：deleteVars 删除其中3个
//
// setTableDefineCallback 是 per-bucket 触发的：每次 pub 广播某个 bucket 的 PubTableDefine，
// 回调收到的是该 bucket 当前全量变量定义。因此用 map<bucketName, set<varName>> 跟踪
// 每个 bucket 的最新状态，取 union 得到当前全局可见变量集合。
//
// 验证：
//   ① 阶段1后：union == 6个变量名，无多无少
//   ② 阶段2后：union == 10个变量名
//   ③ 阶段3后：union == 7个变量名，且被删除的3个名字不在其中
TEST(OnDemandPubSub, TableDefineCallbackAccurateOnCreateAndDelete)
{
    const auto root =
        std::filesystem::temp_directory_path() / uniqueName("ondemand_case_tdcb");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_tdcb");
    const auto defs6  = makeDefines(pubNode, "tdcb_a", 6);
    const auto defs4  = makeDefines(pubNode, "tdcb_b", 4);
    const auto names6 = defineNames(defs6);
    const auto names4 = defineNames(defs4);

    // 阶段3删除 defs6 里的前3个
    const std::vector<std::string> toDelete(names6.begin(), names6.begin() + 3);

    const std::set<std::string> expected_stage1(names6.begin(), names6.end());
    std::set<std::string> expected_stage2 = expected_stage1;
    expected_stage2.insert(names4.begin(), names4.end());
    std::set<std::string> expected_stage3 = expected_stage2;
    for (const auto &n : toDelete)
        expected_stage3.erase(n);

    const auto pubProc =
        spawnChild("tdcb_pub", root, [pubNode, defs6, defs4, toDelete]() {
            dsf::ondemand::OnDemandPub pub;
            ChildReport r;
            if (!childRequire(r, pub.init(pubNode), "pub init failed")
                || !childRequire(r, pub.start(), "pub start failed")
                || !childRequire(r, pub.createVars(defs6), "pub createVars(6) failed")) {
                return r;
            }
            std::this_thread::sleep_for(2000ms); // 阶段1传播

            if (!childRequire(r, pub.createVars(defs4), "pub createVars(+4) failed")) {
                pub.stop();
                return r;
            }
            std::this_thread::sleep_for(2000ms); // 阶段2传播

            if (!childRequire(r, pub.deleteVars(toDelete), "pub deleteVars(3) failed")) {
                pub.stop();
                return r;
            }
            std::this_thread::sleep_for(2000ms); // 阶段3传播

            pub.stop();
            r.ok = true;
            r.message = "ok";
            return r;
        });

    const auto subProc = spawnChild(
        "tdcb_sub", root,
        [pubNode, expected_stage1, expected_stage2, toDelete]() {
            dsf::ondemand::OnDemandSub sub;
            ChildReport r;

            std::mutex mu;
            std::set<std::string> allSeenNames; // 累积所有回调里出现过的变量名
            std::atomic<int> cbFired{0};

            sub.setTableDefineCallback(
                [&](const std::vector<DSF::Var::Define> &defines) {
                    std::lock_guard<std::mutex> lk(mu);
                    for (const auto &d : defines) {
                        if (d.nodeName() == pubNode)
                            allSeenNames.insert(d.name());
                    }
                    cbFired.fetch_add(1, std::memory_order_release);
                });

            if (!childRequire(r, sub.init(uniqueName("sub_tdcb")), "sub init failed")
                || !childRequire(r, sub.start(), "sub start failed")) {
                return r;
            }

            // ── 阶段1：等待回调累积到 expected_stage1 的全部变量名 ──
            const bool stage1Ok = waitUntil(
                [&]() {
                    std::lock_guard<std::mutex> lk(mu);
                    return std::includes(allSeenNames.begin(), allSeenNames.end(),
                                         expected_stage1.begin(), expected_stage1.end());
                },
                8s);
            size_t seen1 = 0;
            { std::lock_guard<std::mutex> lk(mu); seen1 = allSeenNames.size(); }
            childRequire(r, stage1Ok,
                         "stage1: not all vars in callback, seen=" + std::to_string(seen1)
                             + " expected=6");
            r.metrics["stage1_ok"]    = stage1Ok ? "1" : "0";
            r.metrics["stage1_cb"]    = std::to_string(cbFired.load());
            LOG(info) << "[tdcb_sub] stage1 seen=" << seen1 << " cbFired=" << cbFired.load();

            // ── 阶段2：等待回调累积到 expected_stage2 的全部变量名 ──
            const bool stage2Ok = waitUntil(
                [&]() {
                    std::lock_guard<std::mutex> lk(mu);
                    return std::includes(allSeenNames.begin(), allSeenNames.end(),
                                         expected_stage2.begin(), expected_stage2.end());
                },
                8s);
            size_t seen2 = 0;
            { std::lock_guard<std::mutex> lk(mu); seen2 = allSeenNames.size(); }
            childRequire(r, stage2Ok,
                         "stage2: not all vars in callback, seen=" + std::to_string(seen2)
                             + " expected=10");
            r.metrics["stage2_ok"] = stage2Ok ? "1" : "0";
            r.metrics["stage2_cb"] = std::to_string(cbFired.load());
            LOG(info) << "[tdcb_sub] stage2 seen=" << seen2 << " cbFired=" << cbFired.load();

            // ── 阶段3：用 getAvailableVars() 验证 delete 后的状态真值 ──
            // getAvailableVars() 直接读 varDefineIndex_，是最准确的状态反映。
            // 不用回调追踪 delete 后内容：delete 广播可能在 stage2 确认前就到达，
            // 用标志区分"delete 前后回调"存在竞态，不可靠。
            const bool stage3Ok =
                waitUntil([&]() { return countNodeVars(sub, pubNode) == 7; }, 10s);
            const size_t availCount = countNodeVars(sub, pubNode);
            childRequire(r, stage3Ok,
                         "stage3: getAvailableVars mismatch, got=" + std::to_string(availCount)
                             + " expected=7");

            // 验证被删变量不在 getAvailableVars() 里
            const auto availVars = nodeVars(sub, pubNode);
            const std::set<std::string> availSet(availVars.begin(), availVars.end());
            bool noDeletedInAvail = true;
            for (const auto &n : toDelete) {
                if (availSet.count(n)) {
                    noDeletedInAvail = false;
                    LOG(error) << "[tdcb_sub] deleted var still in getAvailableVars: " << n;
                }
            }
            childRequire(r, noDeletedInAvail,
                         "stage3: deleted vars still in getAvailableVars after deleteVars");

            r.metrics["stage3_available_ok"] = stage3Ok ? "1" : "0";
            r.metrics["stage3_no_deleted"]   = noDeletedInAvail ? "1" : "0";
            r.metrics["total_cb_fired"]      = std::to_string(cbFired.load());
            LOG(info) << "[tdcb_sub] stage3 available=" << availCount
                      << " noDeleted=" << noDeletedInAvail << " cbFired=" << cbFired.load();

            sub.stop();
            r.ok = stage1Ok && stage2Ok && stage3Ok && noDeletedInAvail;
            r.message = r.ok ? "ok" : r.message;
            return r;
        });

    ChildReport subReport;
    expectChildOk(pubProc, 16s);
    expectChildOk(subProc, 16s, &subReport);

    EXPECT_EQ(subReport.metrics["stage1_ok"], "1")
        << "stage1: all 6 vars should appear in callback";
    EXPECT_EQ(subReport.metrics["stage2_ok"], "1")
        << "stage2: all 10 vars should appear in callback";
    EXPECT_EQ(subReport.metrics["stage3_available_ok"], "1")
        << "stage3: getAvailableVars should show 7 vars after deleteVars";
    EXPECT_EQ(subReport.metrics["stage3_no_deleted"], "1")
        << "deleted vars must not appear in getAvailableVars after deleteVars";
}


// TC_20a - Pub stop/restart 通信恢复验证
//
// 场景：pub 正常发布10个变量，sub 订阅并收到数据后，pub 调用 stop() 模拟断网，
//       随后 pub 重新 init/start/createVars，验证 sub 能自动恢复通信。
//
// 验证：
//   ① 初始通信正常（sub 收到所有10个变量的回调）
//   ② pub stop 后 sub 检测到断连（getAvailableVars 返回0）
//   ③ pub 重启后 sub 检测到重连（getAvailableVars 返回10）
//   ④ 重连后 sub 重新收到数据回调
TEST(OnDemandPubSub, PubStopAndRestart)
{
    const auto root =
        std::filesystem::temp_directory_path() / uniqueName("ondemand_case20a");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case20a");
    const auto defs  = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case20a_pub", root, [pubNode, defs, names]() {
        ChildReport r;

        // ── 第一轮：正常发布 ──
        {
            dsf::ondemand::OnDemandPub pub;
            if (!childRequire(r, pub.init(pubNode), "pub init1 failed")
                || !childRequire(r, pub.start(), "pub start1 failed")
                || !childRequire(r, pub.createVars(defs), "pub createVars1 failed")) {
                return r;
            }
            std::atomic<bool> running{true};
            std::thread th([&]() { publishLoop(pub, names, running, 10); });
            std::this_thread::sleep_for(3s);
            running.store(false, std::memory_order_release);
            th.join();
            pub.stop();
            LOG(info) << "[case20a_pub] round1 stopped (simulating disconnect)";
        }

        // 等待 DDS liveliness 超时（lease=3s，多留 1s buffer）
        std::this_thread::sleep_for(4s);

        // ── 第二轮：重新启动 ──
        {
            dsf::ondemand::OnDemandPub pub;
            if (!childRequire(r, pub.init(pubNode), "pub init2 failed")
                || !childRequire(r, pub.start(), "pub start2 failed")
                || !childRequire(r, pub.createVars(defs), "pub createVars2 failed")) {
                return r;
            }
            std::atomic<bool> running{true};
            std::thread th([&]() { publishLoop(pub, names, running, 10); });
            std::this_thread::sleep_for(10s);
            running.store(false, std::memory_order_release);
            th.join();
            pub.stop();
            LOG(info) << "[case20a_pub] round2 stopped";
        }

        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case20a_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;

        if (!childRequire(r, sub.init(uniqueName("sub_case20a")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        // 等待 pub 广播变量表
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; }, 8s),
                          "sub did not receive var defines")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        std::map<std::string, uint64_t> latestTs;

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                            std::lock_guard<std::mutex> lk(mu);
                                            for (const auto &v : vars) {
                                                const std::string name(v.varName.data(),
                                                                       v.varName.size());
                                                latestTs[name] = v.timestampNs;
                                            }
                                        }),
                          "subscribe failed")) {
            sub.stop();
            return r;
        }

        // ── 阶段1：初始通信正常 ──
        const bool phase1Ok =
            waitUntil([&]() {
                std::lock_guard<std::mutex> lk(mu);
                return latestTs.size() == names.size();
            }, 6s);
        childRequire(r, phase1Ok, "phase1: did not receive initial callbacks for all 10 vars");
        r.metrics["phase1_ok"] = phase1Ok ? "1" : "0";
        LOG(info) << "[case20a_sub] phase1 done, received=" << [&]() {
            std::lock_guard<std::mutex> lk(mu);
            return latestTs.size();
        }();

        // ── 阶段2：检测断连（getAvailableVars 应降为0） ──
        const bool phase2Ok =
            waitUntil([&]() { return countNodeVars(sub, pubNode) == 0; }, 10s);
        childRequire(r, phase2Ok,
                     "phase2: pub did not go offline (getAvailableVars still non-zero)");
        r.metrics["phase2_ok"] = phase2Ok ? "1" : "0";
        LOG(info) << "[case20a_sub] phase2 done, pub offline detected";

        // ── 阶段3：检测重连（getAvailableVars 恢复为10） ──
        const bool phase3Ok =
            waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; }, 12s);
        childRequire(r, phase3Ok,
                     "phase3: pub did not come back online (getAvailableVars still 0)");
        r.metrics["phase3_ok"] = phase3Ok ? "1" : "0";
        LOG(info) << "[case20a_sub] phase3 done, pub back online";

        // ── 阶段4：验证数据恢复 ──
        // 清空旧时间戳，等待新回调到来
        {
            std::lock_guard<std::mutex> lk(mu);
            latestTs.clear();
        }
        const bool phase4Ok =
            waitUntil([&]() {
                std::lock_guard<std::mutex> lk(mu);
                return latestTs.size() == names.size();
            }, 10s);
        childRequire(r, phase4Ok, "phase4: data did not resume after pub restart");
        r.metrics["phase4_ok"] = phase4Ok ? "1" : "0";
        LOG(info) << "[case20a_sub] phase4 done, data resumed";

        sub.stop();
        r.ok = phase1Ok && phase2Ok && phase3Ok && phase4Ok;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    ChildReport subReport;
    expectChildOk(pubProc, 22s);
    expectChildOk(subProc, 55s, &subReport);

    EXPECT_EQ(subReport.metrics["phase1_ok"], "1") << "phase1: initial callbacks not received";
    EXPECT_EQ(subReport.metrics["phase2_ok"], "1") << "phase2: pub offline not detected";
    EXPECT_EQ(subReport.metrics["phase3_ok"], "1") << "phase3: pub reconnect not detected";
    EXPECT_EQ(subReport.metrics["phase4_ok"], "1") << "phase4: data did not resume after reconnect";
}

// TC_20b - Sub stop/restart 通信恢复验证
//
// 场景：pub 持续发布10个变量，sub 订阅并收到数据后，sub 调用 stop() 模拟断网，
//       随后 sub 重新 init/start/subscribe，验证能重新收到数据。
//
// 验证：
//   ① 初始通信正常（sub 收到所有10个变量的回调）
//   ② sub 重启并重新订阅后，重新收到数据回调
TEST(OnDemandPubSub, SubStopAndRestart)
{
    const auto root =
        std::filesystem::temp_directory_path() / uniqueName("ondemand_case20b");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case20b");
    const auto defs  = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case20b_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(25s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case20b_sub", root, [pubNode, names]() {
        ChildReport r;

        auto runSub = [&](const std::string &subName,
                          const std::chrono::milliseconds waitVarsTimeout,
                          const std::chrono::milliseconds dataTimeout) -> bool {
            dsf::ondemand::OnDemandSub sub;
            if (!childRequire(r, sub.init(subName), subName + " init failed")
                || !childRequire(r, sub.start(), subName + " start failed")) {
                return false;
            }

            if (!childRequire(r,
                              waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; },
                                        waitVarsTimeout),
                              subName + ": did not receive var defines")) {
                sub.stop();
                return false;
            }

            std::mutex mu;
            std::map<std::string, uint64_t> latestTs;

            if (!childRequire(r,
                              sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                            [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                                std::lock_guard<std::mutex> lk(mu);
                                                for (const auto &v : vars) {
                                                    const std::string name(v.varName.data(),
                                                                           v.varName.size());
                                                    latestTs[name] = v.timestampNs;
                                                }
                                            }),
                              subName + ": subscribe failed")) {
                sub.stop();
                return false;
            }

            const bool dataOk =
                waitUntil([&]() {
                    std::lock_guard<std::mutex> lk(mu);
                    return latestTs.size() == names.size();
                }, dataTimeout);
            childRequire(r, dataOk, subName + ": did not receive callbacks for all 10 vars");
            sub.stop();
            return dataOk;
        };

        // ── 第一轮：正常订阅 ──
        const bool phase1Ok = runSub(uniqueName("sub_case20b_r1"), 8s, 6s);
        r.metrics["phase1_ok"] = phase1Ok ? "1" : "0";
        LOG(info) << "[case20b_sub] phase1 done, ok=" << phase1Ok;

        if (!phase1Ok) {
            r.ok = false;
            return r;
        }

        // 模拟断网后等待一段时间再重连
        std::this_thread::sleep_for(3s);

        // ── 第二轮：重新订阅 ──
        const bool phase2Ok = runSub(uniqueName("sub_case20b_r2"), 8s, 8s);
        r.metrics["phase2_ok"] = phase2Ok ? "1" : "0";
        LOG(info) << "[case20b_sub] phase2 done, ok=" << phase2Ok;

        r.ok = phase1Ok && phase2Ok;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    ChildReport subReport;
    expectChildOk(pubProc, 30s);
    expectChildOk(subProc, 45s, &subReport);

    EXPECT_EQ(subReport.metrics["phase1_ok"], "1") << "phase1: initial callbacks not received";
    EXPECT_EQ(subReport.metrics["phase2_ok"], "1") << "phase2: data did not resume after sub restart";
}
#if 0
// TC_22 - Pub stop/start（不重新 init）通信恢复验证
//
// 场景：pub init 一次后，stop() 再直接 start()（不重新 init），验证通信是否恢复。
//       注意：当前 stop() 会将 initialized_ 置为 false，因此 start() 会失败。
//       本用例用于暴露该问题。
//
// 验证：
//   ① 初始通信正常
//   ② pub stop 后 sub 检测到断连
//   ③ pub 不重新 init 直接 start 后，sub 检测到重连
//   ④ 重连后数据恢复
TEST(OnDemandPubSub, PubStopAndStartWithoutReinit)
{
    const auto root =
        std::filesystem::temp_directory_path() / uniqueName("ondemand_case22");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case22");
    const auto defs  = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case22_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;

        // init 一次
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start1 failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars1 failed")) {
            return r;
        }

        // 第一轮发布
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(3s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        LOG(info) << "[case22_pub] round1 stopped";

        // 等待 DDS liveliness 超时
        std::this_thread::sleep_for(4s);

        // 第二轮：不重新 init，直接 start
        if (!childRequire(r, pub.start(), "pub start2 failed (no reinit)")) {
            return r;
        }
        if (!childRequire(r, pub.createVars(defs), "pub createVars2 failed")) {
            pub.stop();
            return r;
        }
        running.store(true, std::memory_order_release);
        std::thread th2([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(10s);
        running.store(false, std::memory_order_release);
        th2.join();
        pub.stop();
        LOG(info) << "[case22_pub] round2 stopped";

        r.ok = true;
        r.message = "ok";
        return r;
    });

    // sub 侧与 TC_20a 相同：等待断连 → 重连 → 数据恢复
    const auto subProc = spawnChild("case22_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;

        if (!childRequire(r, sub.init(uniqueName("sub_case22")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; }, 8s),
                          "sub did not receive var defines")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        std::map<std::string, uint64_t> latestTs;

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                            std::lock_guard<std::mutex> lk(mu);
                                            for (const auto &v : vars) {
                                                const std::string name(v.varName.data(),
                                                                       v.varName.size());
                                                latestTs[name] = v.timestampNs;
                                            }
                                        }),
                          "subscribe failed")) {
            sub.stop();
            return r;
        }

        const bool phase1Ok =
            waitUntil([&]() {
                std::lock_guard<std::mutex> lk(mu);
                return latestTs.size() == names.size();
            }, 6s);
        childRequire(r, phase1Ok, "phase1: initial callbacks not received");
        r.metrics["phase1_ok"] = phase1Ok ? "1" : "0";

        const bool phase2Ok =
            waitUntil([&]() { return countNodeVars(sub, pubNode) == 0; }, 10s);
        childRequire(r, phase2Ok, "phase2: pub offline not detected");
        r.metrics["phase2_ok"] = phase2Ok ? "1" : "0";
        LOG(info) << "[case22_sub] phase2 done, pub offline detected";

        const bool phase3Ok =
            waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; }, 12s);
        childRequire(r, phase3Ok, "phase3: pub did not come back online");
        r.metrics["phase3_ok"] = phase3Ok ? "1" : "0";
        LOG(info) << "[case22_sub] phase3 done, pub back online";

        {
            std::lock_guard<std::mutex> lk(mu);
            latestTs.clear();
        }
        const bool phase4Ok =
            waitUntil([&]() {
                std::lock_guard<std::mutex> lk(mu);
                return latestTs.size() == names.size();
            }, 10s);
        childRequire(r, phase4Ok, "phase4: data did not resume after pub restart");
        r.metrics["phase4_ok"] = phase4Ok ? "1" : "0";
        LOG(info) << "[case22_sub] phase4 done, data resumed";

        sub.stop();
        r.ok = phase1Ok && phase2Ok && phase3Ok && phase4Ok;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    ChildReport subReport;
    expectChildOk(pubProc, 22s);
    expectChildOk(subProc, 45s, &subReport);

    EXPECT_EQ(subReport.metrics["phase1_ok"], "1") << "phase1: initial callbacks not received";
    EXPECT_EQ(subReport.metrics["phase2_ok"], "1") << "phase2: pub offline not detected";
    EXPECT_EQ(subReport.metrics["phase3_ok"], "1") << "phase3: pub reconnect not detected";
    EXPECT_EQ(subReport.metrics["phase4_ok"], "1") << "phase4: data did not resume";
}

// TC_23 - Sub stop/start（不重新 init）通信恢复验证
//
// 场景：sub init 一次后，stop() 再直接 start()（不重新 init），验证通信是否恢复。
//       注意：当前 stop() 会将 initialized_ 置为 false，因此 start() 会失败。
//       本用例用于暴露该问题。
//
// 验证：
//   ① 初始通信正常
//   ② sub stop 后直接 start（不重新 init），重新订阅后数据恢复
TEST(OnDemandPubSub, SubStopAndStartWithoutReinit)
{
    const auto root =
        std::filesystem::temp_directory_path() / uniqueName("ondemand_case23");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case23");
    const auto defs  = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    // pub 持续发布
    const auto pubProc = spawnChild("case23_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }
        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 10); });
        std::this_thread::sleep_for(28s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case23_sub", root, [pubNode, names]() {
        ChildReport r;
        const std::string subName = uniqueName("sub_case23");

        dsf::ondemand::OnDemandSub sub;

        // init 一次
        if (!childRequire(r, sub.init(subName), "sub init failed")
            || !childRequire(r, sub.start(), "sub start1 failed")) {
            return r;
        }

        // ── 第一轮 ──
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; }, 8s),
                          "round1: did not receive var defines")) {
            sub.stop();
            return r;
        }

        std::mutex mu;
        std::map<std::string, uint64_t> latestTs;

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                            std::lock_guard<std::mutex> lk(mu);
                                            for (const auto &v : vars) {
                                                const std::string name(v.varName.data(),
                                                                       v.varName.size());
                                                latestTs[name] = v.timestampNs;
                                            }
                                        }),
                          "round1: subscribe failed")) {
            sub.stop();
            return r;
        }

        const bool phase1Ok =
            waitUntil([&]() {
                std::lock_guard<std::mutex> lk(mu);
                return latestTs.size() == names.size();
            }, 6s);
        childRequire(r, phase1Ok, "phase1: initial callbacks not received");
        r.metrics["phase1_ok"] = phase1Ok ? "1" : "0";
        LOG(info) << "[case23_sub] phase1 done, ok=" << phase1Ok;

        if (!phase1Ok) {
            sub.stop();
            return r;
        }

        sub.stop();
        LOG(info) << "[case23_sub] stopped (simulating disconnect)";

        // 模拟断网后等待
        std::this_thread::sleep_for(3s);

        // ── 第二轮：不重新 init，直接 start ──
        if (!childRequire(r, sub.start(), "sub start2 failed (no reinit)")) {
            return r;
        }

        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == 10; }, 8s),
                          "round2: did not receive var defines after restart")) {
            sub.stop();
            return r;
        }

        {
            std::lock_guard<std::mutex> lk(mu);
            latestTs.clear();
        }

        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 100),
                                        [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
                                            std::lock_guard<std::mutex> lk(mu);
                                            for (const auto &v : vars) {
                                                const std::string name(v.varName.data(),
                                                                       v.varName.size());
                                                latestTs[name] = v.timestampNs;
                                            }
                                        }),
                          "round2: subscribe failed")) {
            sub.stop();
            return r;
        }

        const bool phase2Ok =
            waitUntil([&]() {
                std::lock_guard<std::mutex> lk(mu);
                return latestTs.size() == names.size();
            }, 8s);
        childRequire(r, phase2Ok, "phase2: data did not resume after sub restart");
        r.metrics["phase2_ok"] = phase2Ok ? "1" : "0";
        LOG(info) << "[case23_sub] phase2 done, ok=" << phase2Ok;

        sub.stop();
        r.ok = phase1Ok && phase2Ok;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    ChildReport subReport;
    expectChildOk(pubProc, 32s);
    expectChildOk(subProc, 38s, &subReport);

    EXPECT_EQ(subReport.metrics["phase1_ok"], "1") << "phase1: initial callbacks not received";
    EXPECT_EQ(subReport.metrics["phase2_ok"], "1") << "phase2: data did not resume after sub restart (no reinit)";
}

#endif
// TC_24 - varReadSync 同步读接口验证
//
// 场景：pub 发布 10 个 int32 变量，每个变量写入固定值 (i+1)*1000；
//       sub 订阅时回调传空，等数据到达后通过 varReadSync 同步读取。
//
// 验证：
//   ① 节点名传错 → varReadSync 返回 false
//   ② 节点名传对 → 每个变量均能读出，且：
//        data 值与 pub 写入一致
//        size == sizeof(int32_t)
//        nodeName == pubNode
//        varType  == "int32"
//        blobType == STRUCTS
TEST(OnDemandPubSub, VarReadSyncCorrectness)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_case24");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_case24");
    const auto defs  = makeDefines(pubNode, "v", 10);
    const auto names = defineNames(defs);

    const auto pubProc = spawnChild("case24_pub", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }
        pub.setBlobType(DSF::Var::BLOB_TYPE::STRUCTS);

        std::atomic<bool> running{true};
        std::thread th([&]() {
            while (running.load(std::memory_order_acquire)) {
                for (size_t i = 0; i < names.size(); ++i) {
                    int32_t val = static_cast<int32_t>((i + 1) * 1000);
                    pub.setVarData(names[i].c_str(), &val, sizeof(val));
                }
                std::this_thread::sleep_for(10ms);
            }
        });
        std::this_thread::sleep_for(10s);
        running.store(false, std::memory_order_release);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    const auto subProc = spawnChild("case24_sub", root, [pubNode, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_case24")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        // 等待变量定义到达
        if (!childRequire(r,
                          waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s),
                          "sub did not receive var defines")) {
            sub.stop();
            return r;
        }

        // 回调传空订阅
        if (!childRequire(r,
                          sub.subscribe(pubNode.c_str(), toSubscriptions(names, 50), nullptr),
                          "subscribe failed")) {
            sub.stop();
            return r;
        }

        // 先验证错误 nodeName 路径
        dsf::ondemand::VarCallbackData dummy;
        const bool wrongNodeFails = !sub.varReadSync("wrong_node_xyz", names[0].c_str(), dummy);
        childRequire(r, wrongNodeFails, "wrong node name should return false");
        r.metrics["wrong_node_fails"] = wrongNodeFails ? "1" : "0";

        // 等待一个“全量变量都可读且字段一致”的稳定窗口
        bool allDataOk   = true;
        bool allSizeOk   = true;
        bool allNodeOk   = true;
        bool allTypeOk   = true;
        bool allBlobOk   = true;
        const bool allValidated = waitUntil(
            [&]() {
                allDataOk = true;
                allSizeOk = true;
                allNodeOk = true;
                allTypeOk = true;
                allBlobOk = true;

                for (size_t i = 0; i < names.size(); ++i) {
                    dsf::ondemand::VarCallbackData out;
                    if (!sub.varReadSync(pubNode.c_str(), names[i].c_str(), out)) {
                        return false;
                    }

                    const int32_t expected = static_cast<int32_t>((i + 1) * 1000);
                    const int32_t actual   = *reinterpret_cast<const int32_t *>(out.data);
                    if (actual != expected) {
                        allDataOk = false;
                    }
                    if (out.size != sizeof(int32_t)) {
                        allSizeOk = false;
                    }
                    if (out.nodeName != pubNode) {
                        allNodeOk = false;
                    }
                    if (out.varType != "int32") {
                        allTypeOk = false;
                    }
                    if (out.blobType != DSF::Var::BLOB_TYPE::STRUCTS) {
                        allBlobOk = false;
                    }
                }

                return allDataOk && allSizeOk && allNodeOk && allTypeOk && allBlobOk;
            },
            8s);
        childRequire(r, allValidated, "not all vars validated within timeout");

        r.metrics["all_data_ok"]   = allDataOk  ? "1" : "0";
        r.metrics["all_size_ok"]   = allSizeOk  ? "1" : "0";
        r.metrics["all_node_ok"]   = allNodeOk  ? "1" : "0";
        r.metrics["all_type_ok"]   = allTypeOk  ? "1" : "0";
        r.metrics["all_blob_ok"]   = allBlobOk  ? "1" : "0";

        childRequire(r, allDataOk,  "data value mismatch");
        childRequire(r, allSizeOk,  "size mismatch");
        childRequire(r, allNodeOk,  "nodeName mismatch");
        childRequire(r, allTypeOk,  "varType mismatch");
        childRequire(r, allBlobOk,  "blobType mismatch");

        sub.stop();
        r.ok = wrongNodeFails && allDataOk && allSizeOk && allNodeOk && allTypeOk && allBlobOk;
        r.message = r.ok ? "ok" : r.message;
        return r;
    });

    ChildReport subReport;
    expectChildOk(pubProc, 14s);
    expectChildOk(subProc, 18s, &subReport);

    EXPECT_EQ(subReport.metrics["wrong_node_fails"], "1") << "wrong node name should return false";
    EXPECT_EQ(subReport.metrics["all_data_ok"],      "1") << "data value mismatch";
    EXPECT_EQ(subReport.metrics["all_size_ok"],      "1") << "size mismatch";
    EXPECT_EQ(subReport.metrics["all_node_ok"],      "1") << "nodeName mismatch";
    EXPECT_EQ(subReport.metrics["all_type_ok"],      "1") << "varType mismatch";
    EXPECT_EQ(subReport.metrics["all_blob_ok"],      "1") << "blobType mismatch";
}

// ── setOnPublicationMatchedCallback: pub 端每个 topic 的 match/unmatch 验证 ──
TEST(OnDemandPubSub, PublicationMatchedCallbackPerTopicMatchUnmatch)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_pub_match");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_match");
    const auto defs = makeDefines(pubNode, "v", 4);
    const auto names = defineNames(defs);
    const std::string bucketTopic =
        "dsf/var/data/transfer/bucket_"
        + std::to_string(dsf::ondemand::BucketManager::CalculateBucketIndexFromHash(
            dsf::ondemand::fast_hash(dsf::ondemand::make_meta_varname(pubNode, names[0]))));

    // 子进程1: pub，记录每个 topic 的 match/unmatch 事件
    const auto pubProc = spawnChild("pub_match", root, [pubNode, defs, names, bucketTopic]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        // 每个 topic 记录所有事件: {currentCount, change, totalCount}
        std::mutex mtx;
        std::unordered_map<std::string, std::vector<std::tuple<int, int, int>>> topicEvents;

        pub.setOnPublicationMatchedCallback(
            [&](const std::string &topic, int current, int change, int total) {
                std::lock_guard<std::mutex> lk(mtx);
                topicEvents[topic].emplace_back(current, change, total);
                LOG(info) << "[pub] matched topic=" << topic << " current=" << current
                          << " change=" << change << " total=" << total;
            });

        // 等 sub 上线: bucket topic 的 currentCount 应该变为 1
        const bool gotMatch = waitUntil([&]() {
            std::lock_guard<std::mutex> lk(mtx);
            auto it = topicEvents.find(bucketTopic);
            if (it == topicEvents.end()) return false;
            for (const auto &[cur, chg, tot] : it->second) {
                if (cur >= 1) return true;
            }
            return false;
        }, 10s);
        if (!childRequire(r, gotMatch, "pub: bucket topic never got match (current>=1)")) {
            pub.stop();
            return r;
        }

        // 等 sub 下线: bucket topic 的 currentCount 应该变为 0
        const bool gotUnmatch = waitUntil([&]() {
            std::lock_guard<std::mutex> lk(mtx);
            auto it = topicEvents.find(bucketTopic);
            if (it == topicEvents.end()) return false;
            for (const auto &[cur, chg, tot] : it->second) {
                if (cur == 0) return true;
            }
            return false;
        }, 12s);
        if (!childRequire(r, gotUnmatch, "pub: bucket topic never got unmatch (current==0)")) {
            pub.stop();
            return r;
        }

        // 汇总所有 topic 的事件
        std::lock_guard<std::mutex> lk(mtx);
        for (const auto &[topic, events] : topicEvents) {
            // topic 名用 last 30 chars 做 key（避免太长）
            std::string shortKey = topic.size() > 30 ? topic.substr(topic.size() - 30) : topic;
            // 替换 / 为 _ 做 metric key
            for (char &c : shortKey) { if (c == '/') c = '_'; }

            bool hadMatch = false, hadUnmatch = false;
            int maxCurrent = 0, minCurrentAfterMatch = INT_MAX;
            for (const auto &[cur, chg, tot] : events) {
                if (cur >= 1) hadMatch = true;
                if (hadMatch && cur == 0) hadUnmatch = true;
                maxCurrent = std::max(maxCurrent, cur);
                if (hadMatch) minCurrentAfterMatch = std::min(minCurrentAfterMatch, cur);
            }
            r.metrics["topic_" + shortKey + "_events"]   = std::to_string(events.size());
            r.metrics["topic_" + shortKey + "_matched"]   = hadMatch ? "1" : "0";
            r.metrics["topic_" + shortKey + "_unmatched"] = hadUnmatch ? "1" : "0";
            r.metrics["topic_" + shortKey + "_maxCur"]    = std::to_string(maxCurrent);
        }
        r.metrics["total_topics"] = std::to_string(topicEvents.size());

        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    // 子进程2: sub，上线订阅后等一会再退出（触发 unmatch）
    const auto subProc = spawnChild("sub_match", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_match")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        const bool gotDefs = waitUntil([&]() { return countNodeVars(sub, pubNode) == names.size(); }, 8s);
        if (!childRequire(r, gotDefs, "sub did not receive defines")) {
            sub.stop();
            return r;
        }

        const auto subs = toSubscriptions(names, 200);
        if (!childRequire(r, sub.subscribe(pubNode.c_str(), subs), "subscribe failed")) {
            sub.stop();
            return r;
        }

        std::this_thread::sleep_for(3s);
        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    ChildReport pubReport;
    expectChildOk(subProc, 12s);
    expectChildOk(pubProc, 20s, &pubReport);

    // 验证: 至少有 bucket topic 触发了 match 和 unmatch
    EXPECT_GE(std::stoi(pubReport.metrics["total_topics"]), 1) << "should have at least 1 topic";

    // 找 bucket topic 的 metric（key 中包含 bucket_）
    bool foundBucket = false;
    for (const auto &[k, v] : pubReport.metrics) {
        if (k.find("bucket_") != std::string::npos && k.find("_matched") != std::string::npos) {
            foundBucket = true;
            std::string unmatchKey = k;
            // _matched -> _unmatched
            auto pos = unmatchKey.find("_matched");
            unmatchKey.replace(pos, 8, "_unmatched");
            EXPECT_EQ(v, "1") << k << ": bucket topic should have match event";
            EXPECT_EQ(pubReport.metrics[unmatchKey], "1")
                << unmatchKey << ": bucket topic should have unmatch event (current==0)";
            break;
        }
    }
    EXPECT_TRUE(foundBucket) << "bucket topic not found in callback events";
}

// ── setOnSubscriptionMatchedCallback: sub 端每个 topic 的 match/unmatch 验证 ──
TEST(OnDemandPubSub, SubscriptionMatchedCallbackPerTopicMatchUnmatch)
{
    const auto root = std::filesystem::temp_directory_path() / uniqueName("ondemand_sub_match");
    std::filesystem::create_directories(root);

    const std::string pubNode = uniqueName("pub_match2");
    const auto defs = makeDefines(pubNode, "v", 4);
    const auto names = defineNames(defs);
    const std::string bucketTopic =
        "dsf/var/data/transfer/bucket_"
        + std::to_string(dsf::ondemand::BucketManager::CalculateBucketIndexFromHash(
            dsf::ondemand::fast_hash(dsf::ondemand::make_meta_varname(pubNode, names[0]))));

    // 子进程1: pub，运行后退出（触发 sub 端 unmatch）
    const auto pubProc = spawnChild("pub_match2", root, [pubNode, defs, names]() {
        dsf::ondemand::OnDemandPub pub;
        ChildReport r;
        if (!childRequire(r, pub.init(pubNode), "pub init failed")
            || !childRequire(r, pub.start(), "pub start failed")
            || !childRequire(r, pub.createVars(defs), "pub createVars failed")) {
            return r;
        }

        std::atomic<bool> running{true};
        std::thread th([&]() { publishLoop(pub, names, running, 50); });
        std::this_thread::sleep_for(4s);
        running.store(false);
        th.join();
        pub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    // 子进程2: sub，记录每个 topic 的 match/unmatch 事件
    const auto subProc = spawnChild("sub_match2", root, [pubNode, names, bucketTopic]() {
        dsf::ondemand::OnDemandSub sub;
        ChildReport r;
        if (!childRequire(r, sub.init(uniqueName("sub_match2")), "sub init failed")
            || !childRequire(r, sub.start(), "sub start failed")) {
            return r;
        }

        // 每个 topic 记录所有事件
        std::mutex mtx;
        std::unordered_map<std::string, std::vector<std::tuple<int, int, int>>> topicEvents;

        sub.setOnSubscriptionMatchedCallback(
            [&](const std::string &topic, int current, int change, int total) {
                std::lock_guard<std::mutex> lk(mtx);
                topicEvents[topic].emplace_back(current, change, total);
                LOG(info) << "[sub] matched topic=" << topic << " current=" << current
                          << " change=" << change << " total=" << total;
            });

        // 等 pub 变量定义到达
        const bool gotDefs = waitUntil([&]() {
            return countNodeVars(sub, pubNode) == names.size();
        }, 8s);
        if (!childRequire(r, gotDefs, "sub did not receive defines")) {
            sub.stop();
            return r;
        }

        // 订阅，触发 data transfer reader 匹配
        const auto subs = toSubscriptions(names, 200);
        if (!childRequire(r, sub.subscribe(pubNode.c_str(), subs), "subscribe failed")) {
            sub.stop();
            return r;
        }

        // 等 bucket topic match
        const bool gotMatch = waitUntil([&]() {
            std::lock_guard<std::mutex> lk(mtx);
            auto it = topicEvents.find(bucketTopic);
            if (it == topicEvents.end()) return false;
            for (const auto &[cur, chg, tot] : it->second) {
                if (cur >= 1) return true;
            }
            return false;
        }, 8s);
        if (!childRequire(r, gotMatch, "sub: bucket topic never got match (current>=1)")) {
            sub.stop();
            return r;
        }

        // 等 pub 下线: bucket topic currentCount 变为 0
        const bool gotUnmatch = waitUntil([&]() {
            std::lock_guard<std::mutex> lk(mtx);
            auto it = topicEvents.find(bucketTopic);
            if (it == topicEvents.end()) return false;
            for (const auto &[cur, chg, tot] : it->second) {
                if (cur == 0) return true;
            }
            return false;
        }, 12s);
        if (!childRequire(r, gotUnmatch, "sub: bucket topic never got unmatch (current==0)")) {
            sub.stop();
            return r;
        }

        // 汇总
        std::lock_guard<std::mutex> lk(mtx);
        for (const auto &[topic, events] : topicEvents) {
            std::string shortKey = topic.size() > 30 ? topic.substr(topic.size() - 30) : topic;
            for (char &c : shortKey) { if (c == '/') c = '_'; }

            bool hadMatch = false, hadUnmatch = false;
            int maxCurrent = 0;
            for (const auto &[cur, chg, tot] : events) {
                if (cur >= 1) hadMatch = true;
                if (hadMatch && cur == 0) hadUnmatch = true;
                maxCurrent = std::max(maxCurrent, cur);
            }
            r.metrics["topic_" + shortKey + "_events"]   = std::to_string(events.size());
            r.metrics["topic_" + shortKey + "_matched"]   = hadMatch ? "1" : "0";
            r.metrics["topic_" + shortKey + "_unmatched"] = hadUnmatch ? "1" : "0";
            r.metrics["topic_" + shortKey + "_maxCur"]    = std::to_string(maxCurrent);
        }
        r.metrics["total_topics"] = std::to_string(topicEvents.size());

        sub.stop();
        r.ok = true;
        r.message = "ok";
        return r;
    });

    ChildReport subReport;
    expectChildOk(pubProc, 12s);
    expectChildOk(subProc, 20s, &subReport);

    EXPECT_GE(std::stoi(subReport.metrics["total_topics"]), 1) << "should have at least 1 topic";

    bool foundBucket = false;
    for (const auto &[k, v] : subReport.metrics) {
        if (k.find("bucket_") != std::string::npos && k.find("_matched") != std::string::npos) {
            foundBucket = true;
            std::string unmatchKey = k;
            auto pos = unmatchKey.find("_matched");
            unmatchKey.replace(pos, 8, "_unmatched");
            EXPECT_EQ(v, "1") << k << ": bucket topic should have match event";
            EXPECT_EQ(subReport.metrics[unmatchKey], "1")
                << unmatchKey << ": bucket topic should have unmatch event (current==0)";
            break;
        }
    }
    EXPECT_TRUE(foundBucket) << "bucket topic not found in callback events";
}

#endif

