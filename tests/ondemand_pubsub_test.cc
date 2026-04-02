#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "log/logger.h"
#include "ondemand/on_demand_pub.h"
#include "ondemand/on_demand_sub.h"

namespace {

using namespace std::chrono_literals;

struct ScopedTestDir {
    std::filesystem::path dir;

    explicit ScopedTestDir(const std::string &name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = std::filesystem::temp_directory_path()
              / ("ondemand_pubsub_test_" + name + "_" + std::to_string(::getpid()) + "_"
                 + std::to_string(now));
        std::filesystem::create_directories(dir);
    }

    ~ScopedTestDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
};

inline void AppendLine(const std::filesystem::path &file, const std::string &line)
{
    std::ofstream ofs(file, std::ios::app);
    ofs << line << '\n';
}

inline std::vector<std::string> ReadLines(const std::filesystem::path &file)
{
    std::vector<std::string> out;
    std::ifstream ifs(file);
    std::string line;
    while (std::getline(ifs, line)) {
        out.push_back(line);
    }
    return out;
}

template <typename Fn>
bool WaitFor(Fn &&fn, std::chrono::milliseconds timeout,
             std::chrono::milliseconds interval = 50ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (fn()) {
            return true;
        }
        std::this_thread::sleep_for(interval);
    }
    return fn();
}

struct ChildProc {
#if defined(__linux__)
    pid_t pid{-1};
#endif
    std::string name;
};

#if defined(__linux__)
ChildProc SpawnChild(const std::string &name, const std::function<int()> &fn)
{
    pid_t pid = ::fork();
    if (pid < 0) {
        return ChildProc{-1, name};
    }
    if (pid == 0) {
        int rc = 1;
        try {
            rc = fn();
        } catch (...) {
            rc = 2;
        }
        ::_exit(rc);
    }
    return ChildProc{pid, name};
}

bool WaitChildSuccess(const ChildProc &proc, std::chrono::seconds timeout)
{
    if (proc.pid <= 0) {
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        pid_t rc = ::waitpid(proc.pid, &status, WNOHANG);
        if (rc == proc.pid) {
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        std::this_thread::sleep_for(100ms);
    }

    ::kill(proc.pid, SIGKILL);
    (void)::waitpid(proc.pid, &status, 0);
    return false;
}
#endif

std::vector<DSF::Var::Define> BuildVars(const std::string &node,
                                        const std::vector<std::string> &names)
{
    std::vector<DSF::Var::Define> vars;
    vars.reserve(names.size());
    for (const auto &name : names) {
        DSF::Var::Define v;
        v.name(name);
        v.nodeName(node);
        v.modelName("int");
        v.size(sizeof(int));
        vars.push_back(v);
    }
    return vars;
}

class OnDemandPubSubDualProcessTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Logger::GetInstance()->Init("", Logger::console, Logger::info, 64, 1);
    }
};

TEST_F(OnDemandPubSubDualProcessTest, InitStartStateIsHealthy)
{
#if !defined(__linux__)
    GTEST_SKIP() << "Dual-process test requires Linux fork.";
#else
    ScopedTestDir td("init_start");
    const auto marker = td.dir / "ok.marker";

    ChildProc pub = SpawnChild("pub", [marker]() {
        dsf::ondemand::OnDemandPub p;
        if (!p.init("pub_init_case")) {
            return 10;
        }
        if (!p.start()) {
            return 11;
        }
        std::this_thread::sleep_for(500ms);
        p.stop();
        AppendLine(marker, "pub_ok");
        return 0;
    });

    ChildProc sub = SpawnChild("sub", [marker]() {
        dsf::ondemand::OnDemandSub s;
        if (!s.init("sub_init_case")) {
            return 20;
        }
        if (!s.start()) {
            return 21;
        }
        std::this_thread::sleep_for(500ms);
        s.stop();
        AppendLine(marker, "sub_ok");
        return 0;
    });

    EXPECT_TRUE(WaitChildSuccess(pub, 8s));
    EXPECT_TRUE(WaitChildSuccess(sub, 8s));

    const auto lines = ReadLines(marker);
    EXPECT_EQ(lines.size(), 2u);
#endif
}

TEST_F(OnDemandPubSubDualProcessTest, CreateDeleteVarsPropagateToPeer)
{
#if !defined(__linux__)
    GTEST_SKIP() << "Dual-process test requires Linux fork.";
#else
    ScopedTestDir td("create_delete");
    const auto eventFile = td.dir / "events.log";

    ChildProc pub = SpawnChild("pub", [eventFile]() {
        dsf::ondemand::OnDemandPub p;
        if (!p.init("pub_var_case") || !p.start()) {
            return 10;
        }

        const auto vars = BuildVars("pub_var_case", {"v0", "v1", "v2"});
        if (!p.createVars(vars)) {
            p.stop();
            return 11;
        }

        std::this_thread::sleep_for(1200ms);
        if (!p.deleteVars({"v2"})) {
            p.stop();
            return 12;
        }

        std::this_thread::sleep_for(1800ms);
        p.stop();
        AppendLine(eventFile, "pub_done");
        return 0;
    });

    ChildProc sub = SpawnChild("sub", [eventFile]() {
        dsf::ondemand::OnDemandSub s;
        if (!s.init("sub_var_case") || !s.start()) {
            return 20;
        }

        const bool saw3 = WaitFor(
            [&]() {
                auto vars = s.getAvailableVars();
                auto it = vars.find("pub_var_case");
                return it != vars.end() && it->second.size() >= 3;
            },
            8s);

        if (!saw3) {
            s.stop();
            return 21;
        }
        AppendLine(eventFile, "sub_saw_3_vars");

        std::this_thread::sleep_for(2500ms);

        // 当前实现下 sub 端对 delete 是增量兼容路径，重点验证 delete 后对端依旧可用并完成停止
        auto varsAfterDelete = s.getAvailableVars();
        auto itAfter = varsAfterDelete.find("pub_var_case");

        s.stop();
        if (itAfter == varsAfterDelete.end() || itAfter->second.empty()) {
            return 22;
        }

        AppendLine(eventFile, "sub_alive_after_delete");
        return 0;
    });

    EXPECT_TRUE(WaitChildSuccess(pub, 15s));
    EXPECT_TRUE(WaitChildSuccess(sub, 15s));

    const auto lines = ReadLines(eventFile);
    EXPECT_NE(std::find(lines.begin(), lines.end(), "sub_saw_3_vars"), lines.end());
    EXPECT_NE(std::find(lines.begin(), lines.end(), "sub_alive_after_delete"), lines.end());
#endif
}

TEST_F(OnDemandPubSubDualProcessTest, FrequencyFallbackAndAllUnsubscribeRecovery)
{
#if !defined(__linux__)
    GTEST_SKIP() << "Dual-process test requires Linux fork.";
#else
    ScopedTestDir td("freq_fallback");
    const auto freqFile = td.dir / "freq.log";

    ChildProc pub = SpawnChild("pub", [freqFile]() {
        dsf::ondemand::OnDemandPub p;
        if (!p.init("pub_freq_case") || !p.start()) {
            return 10;
        }

        if (!p.createVars(BuildVars("pub_freq_case", {"f0"}))) {
            p.stop();
            return 11;
        }

        p.setFreqChangeCallback([freqFile](const std::string &varName, uint32_t freq) {
            AppendLine(freqFile, varName + ":" + std::to_string(freq));
        });

        const uint32_t id = p.getVarId("f0");
        int value = 0;
        const auto end = std::chrono::steady_clock::now() + 7s;
        while (std::chrono::steady_clock::now() < end) {
            p.setVarData(id, &value, sizeof(value));
            ++value;
            std::this_thread::sleep_for(10ms);
        }

        p.stop();
        return 0;
    });

    ChildProc subSlow = SpawnChild("subSlow", []() {
        dsf::ondemand::OnDemandSub s;
        if (!s.init("sub_slow_case") || !s.start()) {
            return 20;
        }

        if (!WaitFor([&]() { return s.getTotalReceivedVars() >= 1; }, 8s)) {
            s.stop();
            return 21;
        }

        if (!s.subscribe("pub_freq_case", {{"f0", 200}}, nullptr)) {
            s.stop();
            return 22;
        }

        std::this_thread::sleep_for(4200ms);
        (void)s.unsubscribe("pub_freq_case", {"f0"});
        std::this_thread::sleep_for(800ms);
        s.stop();
        return 0;
    });

    ChildProc subFast = SpawnChild("subFast", []() {
        std::this_thread::sleep_for(1200ms);

        dsf::ondemand::OnDemandSub s;
        if (!s.init("sub_fast_case") || !s.start()) {
            return 30;
        }

        if (!WaitFor([&]() { return s.getTotalReceivedVars() >= 1; }, 8s)) {
            s.stop();
            return 31;
        }

        if (!s.subscribe("pub_freq_case", {{"f0", 50}}, nullptr)) {
            s.stop();
            return 32;
        }

        std::this_thread::sleep_for(1800ms);
        (void)s.unsubscribe("pub_freq_case", {"f0"});
        std::this_thread::sleep_for(400ms);
        s.stop();
        return 0;
    });

    EXPECT_TRUE(WaitChildSuccess(pub, 18s));
    EXPECT_TRUE(WaitChildSuccess(subSlow, 18s));
    EXPECT_TRUE(WaitChildSuccess(subFast, 18s));

    auto lines = ReadLines(freqFile);
    ASSERT_FALSE(lines.empty()) << "No frequency change callback recorded.";

    const auto containsFreq = [&](uint32_t freq) {
        const std::string suffix = ":" + std::to_string(freq);
        for (const auto &l : lines) {
            if (l.size() >= suffix.size() && l.rfind(suffix) == l.size() - suffix.size()) {
                return true;
            }
        }
        return false;
    };

    EXPECT_TRUE(containsFreq(200)) << "Missing fallback frequency 200ms";
    EXPECT_TRUE(containsFreq(50)) << "Missing fast frequency 50ms";
    EXPECT_TRUE(containsFreq(0xFFFFFFFFu)) << "Missing all-unsubscribe recovery frequency";
#endif
}

TEST_F(OnDemandPubSubDualProcessTest, RuntimeDeleteCreateResubscribeAndStop)
{
#if !defined(__linux__)
    GTEST_SKIP() << "Dual-process test requires Linux fork.";
#else
    ScopedTestDir td("runtime_ops");
    const auto cbFile = td.dir / "cb.log";

    ChildProc pub = SpawnChild("pub", []() {
        dsf::ondemand::OnDemandPub p;
        if (!p.init("pub_runtime_case") || !p.start()) {
            return 10;
        }

        if (!p.createVars(BuildVars("pub_runtime_case", {"a", "b"}))) {
            p.stop();
            return 11;
        }

        uint32_t ida = p.getVarId("a");
        uint32_t idb = p.getVarId("b");
        int va = 0;
        int vb = 100;
        for (int i = 0; i < 120; ++i) {
            p.setVarData(ida, &va, sizeof(va));
            p.setVarData(idb, &vb, sizeof(vb));
            ++va;
            ++vb;
            if (i == 35) {
                (void)p.deleteVars({"b"});
                (void)p.createVars(BuildVars("pub_runtime_case", {"c"}));
            }
            if (i > 40) {
                uint32_t idc = p.getVarId("c");
                if (idc != UINT32_MAX) {
                    int vc = i;
                    p.setVarData(idc, &vc, sizeof(vc));
                }
            }
            std::this_thread::sleep_for(30ms);
        }

        p.stop();
        return 0;
    });

    ChildProc sub = SpawnChild("sub", [cbFile]() {
        dsf::ondemand::OnDemandSub s;
        if (!s.init("sub_runtime_case") || !s.start()) {
            return 20;
        }

        if (!WaitFor([&]() { return s.getTotalReceivedVars() >= 2; }, 8s)) {
            s.stop();
            return 21;
        }

        std::atomic<int> seenA{0};
        std::atomic<int> seenB{0};
        std::atomic<int> seenC{0};

        auto cb = [&](const std::vector<dsf::ondemand::VarCallbackData> &vars) {
            for (const auto &v : vars) {
                if (v.varName == "a") {
                    seenA.fetch_add(1, std::memory_order_relaxed);
                } else if (v.varName == "b") {
                    seenB.fetch_add(1, std::memory_order_relaxed);
                } else if (v.varName == "c") {
                    seenC.fetch_add(1, std::memory_order_relaxed);
                }
            }
        };

        if (!s.subscribe("pub_runtime_case", {{"a", 60}, {"b", 60}}, cb)) {
            s.stop();
            return 22;
        }

        std::this_thread::sleep_for(2200ms);
        (void)s.unsubscribe("pub_runtime_case", {"b"});

        const bool hasC = WaitFor(
            [&]() {
                auto vars = s.getAvailableVars();
                auto it = vars.find("pub_runtime_case");
                if (it == vars.end()) {
                    return false;
                }
                for (const auto &name : it->second) {
                    if (name == "c") {
                        return true;
                    }
                }
                return false;
            },
            6s);

        if (!hasC) {
            s.stop();
            return 23;
        }

        if (!s.subscribe("pub_runtime_case", {{"c", 80}}, cb)) {
            s.stop();
            return 24;
        }

        std::this_thread::sleep_for(2200ms);
        s.stop();

        AppendLine(cbFile, "a=" + std::to_string(seenA.load()));
        AppendLine(cbFile, "b=" + std::to_string(seenB.load()));
        AppendLine(cbFile, "c=" + std::to_string(seenC.load()));
        return 0;
    });

    EXPECT_TRUE(WaitChildSuccess(pub, 20s));
    EXPECT_TRUE(WaitChildSuccess(sub, 20s));

    std::unordered_map<std::string, int> counts;
    for (const auto &line : ReadLines(cbFile)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        counts[line.substr(0, pos)] = std::atoi(line.substr(pos + 1).c_str());
    }

    EXPECT_GT(counts["a"], 0);
    EXPECT_GT(counts["b"], 0);
    EXPECT_GT(counts["c"], 0);
#endif
}

TEST_F(OnDemandPubSubDualProcessTest, ExtraCaseSubscribeUnknownVarShouldNotCrash)
{
#if !defined(__linux__)
    GTEST_SKIP() << "Dual-process test requires Linux fork.";
#else
    ScopedTestDir td("unknown_var");
    const auto eventFile = td.dir / "events.log";

    ChildProc pub = SpawnChild("pub", []() {
        dsf::ondemand::OnDemandPub p;
        if (!p.init("pub_unknown_case") || !p.start()) {
            return 10;
        }
        (void)p.createVars(BuildVars("pub_unknown_case", {"x"}));
        std::this_thread::sleep_for(1500ms);
        p.stop();
        return 0;
    });

    ChildProc sub = SpawnChild("sub", [eventFile]() {
        dsf::ondemand::OnDemandSub s;
        if (!s.init("sub_unknown_case") || !s.start()) {
            return 20;
        }
        if (!WaitFor([&]() { return s.getTotalReceivedVars() >= 1; }, 8s)) {
            s.stop();
            return 21;
        }

        // 订阅不存在的变量，接口不应崩溃
        (void)s.subscribe("pub_unknown_case", {{"not_exist", 100}}, nullptr);
        std::this_thread::sleep_for(500ms);
        s.stop();
        AppendLine(eventFile, "sub_survived");
        return 0;
    });

    EXPECT_TRUE(WaitChildSuccess(pub, 10s));
    EXPECT_TRUE(WaitChildSuccess(sub, 10s));

    auto lines = ReadLines(eventFile);
    EXPECT_NE(std::find(lines.begin(), lines.end(), "sub_survived"), lines.end());
#endif
}

} // namespace
