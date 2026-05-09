#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "dds_wrapper/dds_abstraction.h"
#include "log/logger.h"

#ifdef USE_FASTDDS
#include "fastdds_test_idl/PerformanceTest.hpp"
#include "fastdds_test_idl/PerformanceTestPubSubTypes.hpp"
#elif defined(USE_TXDDS)
#include "txdds_test_idl/PerformanceTest.h"
#include "txdds_test_idl/PerformanceTestPubSubTypes.h"
#else
#error "Please define either USE_FASTDDS or USE_TXDDS to select DDS implementation"
#endif

using namespace PerformanceTest;

namespace {

using TestNode = DdsWrapper::DataNode;

template <typename T>
using TestWriter = DdsWrapper::DDSTopicWriter<T>;

struct ScopedTestDir {
    std::filesystem::path dir;

    explicit ScopedTestDir(const std::string &name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = std::filesystem::temp_directory_path()
              / ("dds_wrapper_test_" + name + "_" + std::to_string(::getpid()) + "_"
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
             std::chrono::milliseconds interval = std::chrono::milliseconds(50))
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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ::kill(proc.pid, SIGKILL);
    (void)::waitpid(proc.pid, &status, 0);
    return false;
}
#endif

class DdsWrapperNodeTEST : public ::testing::Test {
protected:
    void SetUp() override { Logger::GetInstance()->Init("", Logger::console, Logger::info, 64, 1); }
    void TearDown() override {}
};

TEST_F(DdsWrapperNodeTEST, sigDefaultNodeSubPubTest)
{
    TestNode node(66, "test_reader");
    EXPECT_TRUE(node.isInitialized());

    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "sigDefaultNodeSubPubTest_topic",
        [](const std::string &topic, std::shared_ptr<TestMessage> data) {
            LOG(info) << "Received data on topic: " << topic
                      << " with message: " << data->message_type();
        });
    EXPECT_TRUE(reader != nullptr);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("sigDefaultNodeSubPubTest_topic");
    EXPECT_TRUE(writer != nullptr);

    TestMessage message;
    message.message_type("Hello, DDS wrapper!");
    uint32_t write_count = 0;
    while (write_count < 5) {
        if (writer->writeMessage(message)) {
            LOG(info) << "Sent message: " << message.message_type();
            write_count++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

TEST_F(DdsWrapperNodeTEST, UserReleaseWriterTest)
{
    TestNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer, nullptr);
    EXPECT_EQ(writer.use_count(), 1) << "Initial reference count should be 1";

    LOG(info) << "Writer created, use_count = " << writer.use_count();
    writer.reset();
    LOG(info) << "Writer reset, use_count = " << (writer ? writer.use_count() : 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto writer2 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer2, nullptr) << "Should be able to recreate writer after reset";
    EXPECT_EQ(writer2.use_count(), 1) << "New writer reference count should be 1";

    LOG(info) << "用户释放后引用计数正确清0，可以重新创建";
}

TEST_F(DdsWrapperNodeTEST, UserReleaseReaderTest)
{
    TestNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    auto callback = [](const std::string &, std::shared_ptr<TestMessage>) {
        // 回调函数
    };

    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>("test_topic", callback);
    ASSERT_NE(reader, nullptr);
    EXPECT_EQ(reader.use_count(), 1) << "Initial reference count should be 1";

    LOG(info) << "Reader created, use_count = " << reader.use_count();
    reader.reset();
    LOG(info) << "Reader reset, use_count = " << (reader ? reader.use_count() : 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto reader2 = node.createDataReader<TestMessage, TestMessagePubSubType>("test_topic", callback);
    ASSERT_NE(reader2, nullptr) << "Should be able to recreate reader after reset";
    EXPECT_EQ(reader2.use_count(), 1) << "New reader reference count should be 1";

    LOG(info) << "用户释放reader后引用计数正确清0，可以重新创建";
}

TEST_F(DdsWrapperNodeTEST, NodeDestructorCleanupTest)
{
    std::weak_ptr<void> writer_weak;
    std::weak_ptr<void> reader_weak;

    {
        TestNode temp_node(0, "test_participant");
        ASSERT_TRUE(temp_node.isInitialized());

        auto writer = temp_node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic_w");
        ASSERT_NE(writer, nullptr);
        LOG(info) << "Writer created, use_count = " << writer.use_count();

        auto callback = [](const std::string &, std::shared_ptr<TestMessage>) {};
        auto reader = temp_node.createDataReader<TestMessage, TestMessagePubSubType>("test_topic_r", callback);
        ASSERT_NE(reader, nullptr);
        LOG(info) << "Reader created, use_count = " << reader.use_count();

        writer_weak = writer;
        reader_weak = reader;

        EXPECT_FALSE(writer_weak.expired()) << "Writer should be alive";
        EXPECT_FALSE(reader_weak.expired()) << "Reader should be alive";

        LOG(info) << "Node going out of scope with user still holding writer/reader...";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(writer_weak.expired()) << "Writer should be expired after node destruction";
    EXPECT_TRUE(reader_weak.expired()) << "Reader should be expired after node destruction";

    LOG(info) << "Node析构时正确释放了用户未释放的writer/reader";
}

TEST_F(DdsWrapperNodeTEST, ReferenceCountWithoutUserReleaseTest)
{
    std::shared_ptr<TestWriter<TestMessage>> writer;
    std::weak_ptr<void> writer_weak;

    {
        TestNode temp_node(0, "test_participant");
        ASSERT_TRUE(temp_node.isInitialized());

        writer = temp_node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
        ASSERT_NE(writer, nullptr);

        EXPECT_EQ(writer.use_count(), 1) << "Reference count should be 1 (only external holder)";
        LOG(info) << "Writer use_count (before node destruction) = " << writer.use_count();

        writer_weak = writer;
        EXPECT_FALSE(writer_weak.expired());

        LOG(info) << "Destroying node...";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(writer.use_count(), 1) << "Reference count should still be 1 (external holder only)";
    EXPECT_FALSE(writer_weak.expired()) << "Writer should still be alive (held by external)";
    LOG(info) << "Writer use_count (after node destruction) = " << writer.use_count();

    writer.reset();
    EXPECT_TRUE(writer_weak.expired()) << "Writer should be expired after user release";
    LOG(info) << "引用计数变化符合预期：Node使用weak_ptr不影响外部shared_ptr";
}

TEST_F(DdsWrapperNodeTEST, DuplicateWriterProtectionTest)
{
    TestNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    auto writer1 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer1, nullptr);

    auto writer2 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    EXPECT_EQ(writer2, nullptr) << "Should not be able to create duplicate writer";

    LOG(info) << "重复创建同名writer被正确拒绝";

    writer1.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto writer3 = node.createDataWriter<TestMessage, TestMessagePubSubType>("test_topic");
    ASSERT_NE(writer3, nullptr) << "Should be able to create after release";

    LOG(info) << "重复创建保护与释放后重建行为正常";
}

TEST_F(DdsWrapperNodeTEST, MultipleWriterReaderLifecycleTest)
{
    TestNode node(0, "test_participant");
    ASSERT_TRUE(node.isInitialized());

    auto writer1 = node.createDataWriter<TestMessage, TestMessagePubSubType>("topic1");
    auto writer2 = node.createDataWriter<TestMessage, TestMessagePubSubType>("topic2");
    ASSERT_NE(writer1, nullptr);
    ASSERT_NE(writer2, nullptr);

    auto callback = [](const std::string &, std::shared_ptr<TestMessage>) {};
    auto reader1 = node.createDataReader<TestMessage, TestMessagePubSubType>("topic3", callback);
    auto reader2 = node.createDataReader<TestMessage, TestMessagePubSubType>("topic4", callback);
    ASSERT_NE(reader1, nullptr);
    ASSERT_NE(reader2, nullptr);

    EXPECT_EQ(writer1.use_count(), 1);
    EXPECT_EQ(writer2.use_count(), 1);
    EXPECT_EQ(reader1.use_count(), 1);
    EXPECT_EQ(reader2.use_count(), 1);

    writer1.reset();
    reader1.reset();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto writer1_new = node.createDataWriter<TestMessage, TestMessagePubSubType>("topic1");
    auto reader1_new = node.createDataReader<TestMessage, TestMessagePubSubType>("topic3", callback);

    ASSERT_NE(writer1_new, nullptr);
    ASSERT_NE(reader1_new, nullptr);

    LOG(info) << "多个writer/reader的生命周期管理正确";
}

} // namespace
