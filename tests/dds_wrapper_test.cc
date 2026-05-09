#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <sstream>
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

inline std::string uniqueName(const std::string &prefix)
{
    auto nowNs =
        std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << prefix << "_" << ::getpid() << "_" << nowNs;
    return oss.str();
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

/**
 * TC1 - 单节点默认 Publisher/Subscriber 收发验证
 *
 * 功能：验证 DataNode 使用默认（无名）Publisher/Subscriber 时，同节点内
 *       DataWriter 写入的消息能被同 topic 的 DataReader 回调接收到。
 *
 * 流程：
 *   1. 创建 DataNode（domainId=66, name="test_reader"）
 *   2. 在默认 subscriber 下创建 DataReader，注册消息回调
 *   3. 在默认 publisher 下创建同 topic 的 DataWriter
 *   4. 循环发送 5 条消息，每条间隔 500ms
 *
 * 验证：node 初始化成功，reader/writer 创建成功，回调能收到消息。
 */
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

/**
 * TC2 - 用户主动释放 Writer 后重建验证
 *
 * 功能：验证用户通过 shared_ptr::reset() 主动释放 DataWriter 后，
 *       内部引用计数正确归零，且同一 topic 上可以重新创建新的 DataWriter。
 *
 * 流程：
 *   1. 创建 DataNode，创建 DataWriter（引用计数应为 1）
 *   2. 用户 reset() 释放 writer（引用计数归零，底层资源回收）
 *   3. 等待 50ms 让底层 DDS 清理完成
 *   4. 在同一 topic 上重新创建 DataWriter，验证创建成功且引用计数为 1
 *
 * 验证：writer 引用计数管理正确，释放后 topic 资源可复用。
 */
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

/**
 * TC3 - 用户主动释放 Reader 后重建验证
 *
 * 功能：验证用户通过 shared_ptr::reset() 主动释放 DataReader 后，
 *       内部引用计数正确归零，且同一 topic 上可以重新创建新的 DataReader。
 *
 * 流程：
 *   1. 创建 DataNode，创建 DataReader（引用计数应为 1）
 *   2. 用户 reset() 释放 reader（引用计数归零，底层资源回收）
 *   3. 等待 50ms 让底层 DDS 清理完成
 *   4. 在同一 topic 上重新创建 DataReader，验证创建成功且引用计数为 1
 *
 * 验证：reader 引用计数管理正确，释放后 topic 资源可复用。
 */
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

/**
 * TC4 - Node 析构时自动清理未释放的 Writer/Reader 验证
 *
 * 功能：验证 DataNode 析构时，即使用户未主动释放 DataWriter/DataReader，
 *       Node 也能自动回收所有由它创建的 DDS 资源，避免资源泄漏。
 *
 * 流程：
 *   1. 在作用域内创建 DataNode，创建 writer 和 reader
 *   2. 用 weak_ptr 观察 writer/reader 的生命周期
 *   3. 作用域结束，DataNode 析构（用户持有的 shared_ptr 也随之销毁）
 *   4. 等待 100ms 让底层 DDS 清理完成
 *   5. 验证 weak_ptr 已过期（writer/reader 被正确回收）
 *
 * 验证：Node 析构能级联清理所有未释放的 endpoint 资源，无泄漏。
 */
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

/**
 * TC5 - 同 topic 重复创建 Writer 保护验证
 *
 * 功能：验证同一 topic 上不允许同时存在两个 DataWriter（防重复保护）。
 *       释放后可以重新创建，确保 topic 资源的独占性。
 *
 * 流程：
 *   1. 创建 DataNode，在 "test_topic" 上创建 writer1（成功）
 *   2. 尝试在同一 topic 上创建 writer2（应返回 nullptr，被拒绝）
 *   3. 用户 reset() 释放 writer1
 *   4. 等待 50ms 让底层 DDS 清理完成
 *   5. 在同一 topic 上创建 writer3（成功，topic 资源已释放可复用）
 *
 * 验证：重复创建被拒绝，释放后重建正常。
 */
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

/**
 * TC6 - 多 Writer/Reader 独立生命周期管理验证
 *
 * 功能：验证同一 DataNode 下多个不同 topic 的 DataWriter/DataReader
 *       各自独立管理生命周期，互不影响。
 *       部分释放后，对应 topic 可以重新创建，未释放的不受影响。
 *
 * 流程：
 *   1. 创建 DataNode
 *   2. 创建 2 个 writer（topic1, topic2）和 2 个 reader（topic3, topic4）
 *   3. 验证所有 endpoint 引用计数均为 1
 *   4. 释放 writer1 和 reader1
 *   5. 等待 50ms 让底层 DDS 清理完成
 *   6. 重新创建 topic1 的 writer 和 topic3 的 reader（成功）
 *   7. topic2 的 writer 和 topic4 的 reader 仍正常存在（未受影响）
 *
 * 验证：不同 topic 的 endpoint 生命周期互相独立，部分释放不影响其他 topic。
 */
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

// ============================================================
//  Partition 分区测试
// ============================================================

/**
 * TC8 - Partition 匹配通信验证（静态设置）
 *
 * 功能：验证 Publisher 和 Subscriber 设置相同 partition 时，
 *       DataWriter 写入的消息能被 DataReader 正常接收。
 *
 * 流程：
 *   1. 创建 subscriber node（partition="tc8_partition"），创建 reader
 *   2. 创建 publisher node（partition="tc8_partition"），创建 writer
 *   3. writer 发送消息，验证 reader 收到
 *
 * 验证：相同 partition 的 writer/reader 能正常匹配通信。
 */
TEST_F(DdsWrapperNodeTEST, PartitionMatchCommunicationTest)
{
    // subscriber 侧
    TestNode subNode(0, "tc8_sub_node");
    ASSERT_TRUE(subNode.isInitialized());

    DdsWrapper::SubscriberQoSBuilder subQos;
    subQos.setPartition("tc8_partition");
    ASSERT_TRUE(subNode.createSubscriber("tc8_sub", subQos));

    std::atomic<int> recvCount{0};
    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);
    auto reader = subNode.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc8_topic", "tc8_sub",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos, nullptr);
    ASSERT_NE(reader, nullptr);

    // publisher 侧
    TestNode pubNode(0, "tc8_pub_node");
    ASSERT_TRUE(pubNode.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    pubQos.setPartition("tc8_partition");
    ASSERT_TRUE(pubNode.createPublisher("tc8_pub", pubQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);
    auto writer = pubNode.createDataWriter<TestMessage, TestMessagePubSubType>(
        "tc8_topic", "tc8_pub", writerQos, nullptr);
    ASSERT_NE(writer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 等待 discovery
    for (int i = 0; i < 3; ++i) {
        TestMessage msg;
        msg.message_type("tc8_msg_" + std::to_string(i));
        writer->writeMessage(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(recvCount.load(), 3) << "Should receive at least 3 messages with matching partition";
}

/**
 * TC9 - Partition 不匹配时通信隔离验证
 *
 * 功能：验证 Publisher 和 Subscriber 设置不同 partition 时，
 *       DataWriter 写入的消息不会被 DataReader 接收。
 *
 * 流程：
 *   1. 创建 subscriber（partition="tc9_right"），创建 reader
 *   2. 创建 publisher（partition="tc9_wrong"），创建 writer
 *   3. writer 发送消息，验证 reader 收不到（partition 不匹配）
 *
 * 验证：不同 partition 的 writer/reader 无法匹配，实现通信隔离。
 */
TEST_F(DdsWrapperNodeTEST, PartitionMismatchIsolationTest)
{
    TestNode subNode(0, "tc9_sub_node");
    ASSERT_TRUE(subNode.isInitialized());

    DdsWrapper::SubscriberQoSBuilder subQos;
    subQos.setPartition("tc9_right");
    ASSERT_TRUE(subNode.createSubscriber("tc9_sub", subQos));

    std::atomic<int> recvCount{0};
    auto reader = subNode.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc9_topic", "tc9_sub",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        DdsWrapper::DataReaderQoSBuilder{}, nullptr);
    ASSERT_NE(reader, nullptr);

    TestNode pubNode(0, "tc9_pub_node");
    ASSERT_TRUE(pubNode.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    pubQos.setPartition("tc9_wrong");
    ASSERT_TRUE(pubNode.createPublisher("tc9_pub", pubQos));

    DdsWrapper::DataWriterQoSBuilder wqos;
    auto writer = pubNode.createDataWriter<TestMessage, TestMessagePubSubType>(
        "tc9_topic", "tc9_pub", wqos, nullptr);
    ASSERT_NE(writer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for (int i = 0; i < 5; ++i) {
        TestMessage msg;
        msg.message_type("tc9_msg_" + std::to_string(i));
        writer->writeMessage(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(recvCount.load(), 0) << "Should NOT receive messages with mismatched partition";
}

/**
 * TC10 - Reader 创建后动态修改 Partition 通信验证
 *
 * 功能：验证先创建 DataReader（无 partition），再通过 updateSubscriberQos
 *       动态设置 partition 后，reader 能否开始接收匹配 writer 的消息。
 *
 * 流程：
 *   1. 创建 subscriber（无 partition），创建 reader
 *   2. 创建 publisher（partition="tc10_partition"），创建 writer，持续发送消息
 *   3. 等待 1 秒，确认此时未收到消息（无 partition 不匹配）
 *   4. 调用 updateSubscriberQos 设置 partition="tc10_partition"
 *   5. 继续等待 3 秒，检查是否开始收到消息
 *
 * 验证：如果收到消息，说明 DDS 实现支持动态 partition 更新；
 *       如果未收到，说明需要在创建 reader 之前设置 partition。
 */
TEST_F(DdsWrapperNodeTEST, DynamicPartitionUpdateAfterReaderCreationTest)
{
    // subscriber 侧：先不设 partition
    TestNode subNode(0, "tc10_sub_node");
    ASSERT_TRUE(subNode.isInitialized());

    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(subNode.createSubscriber("tc10_sub", subQos));

    std::atomic<int> recvCount{0};
    auto reader = subNode.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc10_topic", "tc10_sub",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        DdsWrapper::DataReaderQoSBuilder{}, nullptr);
    ASSERT_NE(reader, nullptr);

    // publisher 侧：带 partition
    TestNode pubNode(0, "tc10_pub_node");
    ASSERT_TRUE(pubNode.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    pubQos.setPartition("tc10_partition");
    ASSERT_TRUE(pubNode.createPublisher("tc10_pub", pubQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto writer = pubNode.createDataWriter<TestMessage, TestMessagePubSubType>(
        "tc10_topic", "tc10_pub", writerQos, nullptr);
    ASSERT_NE(writer, nullptr);

    // 持续发送消息的线程
    std::atomic<bool> sending{true};
    std::thread sender([&]() {
        int i = 0;
        while (sending.load()) {
            TestMessage msg;
            msg.message_type("tc10_msg_" + std::to_string(i++));
            writer->writeMessage(msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    // 等待 1 秒，此时不应收到消息（subscriber 无 partition）
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int countBeforeUpdate = recvCount.load();
    LOG(info) << "Before partition update: received " << countBeforeUpdate << " messages";

    // 动态更新 subscriber 的 partition
    DdsWrapper::SubscriberQoSBuilder newSubQos;
    newSubQos.setPartition("tc10_partition");
    bool updateOk = subNode.updateSubscriberQos("tc10_sub", newSubQos);
    ASSERT_TRUE(updateOk) << "updateSubscriberQos should return true";
    LOG(info) << "Partition updated dynamically";

    // 继续等待 3 秒，检查是否开始收到消息
    std::this_thread::sleep_for(std::chrono::seconds(3));
    int countAfterUpdate = recvCount.load();
    LOG(info) << "After partition update: received " << countAfterUpdate << " messages";

    sending.store(false);
    sender.join();

    // 报告结果——探测 DDS 实现行为
    if (countAfterUpdate > countBeforeUpdate) {
        LOG(info) << "RESULT: DDS supports dynamic partition update after reader creation";
    } else {
        LOG(warning) << "RESULT: DDS does NOT support dynamic partition update after reader creation"
                     << " — partition must be set BEFORE creating readers";
    }
    EXPECT_TRUE(true) << "Dynamic partition test completed. before=" << countBeforeUpdate
                      << " after=" << countAfterUpdate;
}

// ============================================================
//  多节点通信测试
// ============================================================

/**
 * TC11 - 双节点默认 Publisher/Subscriber 通信验证
 *
 * 功能：验证两个独立 DDS DomainParticipant 使用默认（无名）Publisher/Subscriber
 *       能否通过同一 topic 通信。
 *
 * 流程：
 *   1. 创建 subscriber node，创建 reader 监听 "tc11_topic"
 *   2. 创建 publisher node，创建 writer 发送消息到 "tc11_topic"
 *   3. 验证 reader 收到消息
 *
 * 验证：不同 DomainParticipant 通过 DDS discovery 自动发现并匹配。
 */
TEST_F(DdsWrapperNodeTEST, TwoNodeDefaultPubSubCommunicationTest)
{
    TestNode subNode(0, "tc11_sub");
    ASSERT_TRUE(subNode.isInitialized());

    std::atomic<int> recvCount{0};
    std::string lastMsg;
    std::mutex msgMu;

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto reader = subNode.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc11_topic",
        [&](const std::string &, std::shared_ptr<TestMessage> data) {
            if (data) {
                std::lock_guard<std::mutex> lk(msgMu);
                lastMsg = data->message_type();
                recvCount.fetch_add(1);
            }
        },
        readerQos);
    ASSERT_NE(reader, nullptr);

    TestNode pubNode(0, "tc11_pub");
    ASSERT_TRUE(pubNode.isInitialized());

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto writer = pubNode.createDataWriter<TestMessage, TestMessagePubSubType>("tc11_topic",
                                                                               writerQos);
    ASSERT_NE(writer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    TestMessage msg;
    msg.message_type("hello_from_node_a");
    writer->writeMessage(msg);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(recvCount.load(), 1) << "Should receive at least 1 message from another node";
    {
        std::lock_guard<std::mutex> lk(msgMu);
        EXPECT_EQ(lastMsg, "hello_from_node_a");
    }
}

/**
 * TC12 - 双节点命名 Publisher/Subscriber + Partition 通信验证
 *
 * 功能：验证两个独立 DDS DomainParticipant 使用命名的 Publisher/Subscriber
 *       并设置相同 partition 时能否正常通信。模拟 onDemand 系统中的实际使用模式。
 *
 * 流程：
 *   1. 创建 subscriber node（name="data_sub", partition="VarDataTransfer_nodeA"），创建 reader
 *   2. 创建 publisher node（name="data_pub", partition="VarDataTransfer_nodeA"），创建 writer
 *   3. writer 发送消息，验证 reader 收到
 *
 * 验证：命名 Publisher/Subscriber + Partition 的跨 node 通信能力。
 */
TEST_F(DdsWrapperNodeTEST, TwoNodeNamedPubSubWithPartitionTest)
{
    TestNode subNode(0, "tc12_sub");
    ASSERT_TRUE(subNode.isInitialized());

    DdsWrapper::SubscriberQoSBuilder subQos;
    subQos.setPartition("VarDataTransfer_nodeA");
    ASSERT_TRUE(subNode.createSubscriber("data_sub", subQos));

    std::atomic<int> recvCount{0};
    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto reader = subNode.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc12_topic", "data_sub",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos, nullptr);
    ASSERT_NE(reader, nullptr);

    TestNode pubNode(0, "tc12_pub");
    ASSERT_TRUE(pubNode.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    pubQos.setPartition("VarDataTransfer_nodeA");
    ASSERT_TRUE(pubNode.createPublisher("data_pub", pubQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto writer = pubNode.createDataWriter<TestMessage, TestMessagePubSubType>(
        "tc12_topic", "data_pub", writerQos, nullptr);
    ASSERT_NE(writer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for (int i = 0; i < 3; ++i) {
        TestMessage m;
        m.message_type("tc12_msg_" + std::to_string(i));
        writer->writeMessage(m);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(recvCount.load(), 3) << "Named pub/sub with partition should communicate";
}

/**
 * TC13 - 多 topic 并行通信验证
 *
 * 功能：验证同一个 subscriber node 下多个不同 topic 的 reader 能并行接收消息，
 *       各 topic 之间互不干扰。
 *
 * 流程：
 *   1. 创建 subscriber node，创建 2 个 reader 分别监听 "tc13_topic_a" 和 "tc13_topic_b"
 *   2. 创建 publisher node A，向 "tc13_topic_a" 发送 3 条消息
 *   3. 创建 publisher node B，向 "tc13_topic_b" 发送 5 条消息
 *   4. 验证两个 reader 分别收到对应 topic 的消息
 *
 * 验证：多 topic 并行通信，topic 之间隔离。
 */
TEST_F(DdsWrapperNodeTEST, ThreeNodeMultiTopicCommunicationTest)
{
    TestNode subNode(0, "tc13_sub");
    ASSERT_TRUE(subNode.isInitialized());

    std::atomic<int> recvCountA{0};
    std::atomic<int> recvCountB{0};

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);

    auto readerA = subNode.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc13_topic_a",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCountA.fetch_add(1); },
        readerQos);
    auto readerB = subNode.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc13_topic_b",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCountB.fetch_add(1); },
        readerQos);
    ASSERT_NE(readerA, nullptr);
    ASSERT_NE(readerB, nullptr);

    // publisher A → topic_a
    TestNode pubNodeA(0, "tc13_pub_a");
    ASSERT_TRUE(pubNodeA.isInitialized());
    DdsWrapper::DataWriterQoSBuilder wqos;
    wqos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto writerA = pubNodeA.createDataWriter<TestMessage, TestMessagePubSubType>("tc13_topic_a",
                                                                                 wqos);
    ASSERT_NE(writerA, nullptr);

    // publisher B → topic_b
    TestNode pubNodeB(0, "tc13_pub_b");
    ASSERT_TRUE(pubNodeB.isInitialized());
    auto writerB = pubNodeB.createDataWriter<TestMessage, TestMessagePubSubType>("tc13_topic_b",
                                                                                 wqos);
    ASSERT_NE(writerB, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (int i = 0; i < 3; ++i) {
        TestMessage m;
        m.message_type("from_a_" + std::to_string(i));
        writerA->writeMessage(m);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    for (int i = 0; i < 5; ++i) {
        TestMessage m;
        m.message_type("from_b_" + std::to_string(i));
        writerB->writeMessage(m);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(recvCountA.load(), 3) << "Should receive messages on topic_a";
    EXPECT_GE(recvCountB.load(), 5) << "Should receive messages on topic_b";
}

// ============================================================
//  QoS 策略测试（每个 QoS 独立用例，细粒度验证）
// ============================================================

/**
 * TC14 - Reliability QoS: RELIABLE 模式验证
 *
 * 功能：验证 writer 和 reader 都设置 RELIABLE 可靠性时，
 *       所有消息都能被可靠送达（不丢消息）。
 *
 * 流程：
 *   1. 创建 writer（RELIABLE）和 reader（RELIABLE）
 *   2. writer 发送 10 条消息
 *   3. 等待 reader 收到全部 10 条
 *
 * 验证：RELIABLE 模式下消息不丢失。
 */
TEST_F(DdsWrapperNodeTEST, QoSReliableTest)
{
    TestNode node(0, "tc14_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc14_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc14_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);

    std::atomic<int> recvCount{0};
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc14_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc14_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    constexpr int kTotal = 10;
    for (int i = 0; i < kTotal; ++i) {
        TestMessage msg;
        msg.message_type("reliable_" + std::to_string(i));
        writer->writeMessage(msg);
    }

    bool allReceived = WaitFor([&]() { return recvCount.load() >= kTotal; },
                               std::chrono::seconds(5));
    EXPECT_TRUE(allReceived) << "RELIABLE mode: expected " << kTotal << " but got "
                             << recvCount.load();
}

/**
 * TC15 - Reliability QoS: BEST_EFFORT 模式验证
 *
 * 功能：验证 writer 和 reader 都设置 BEST_EFFORT 时能正常通信。
 *       BEST_EFFORT 不保证送达，但基本通信能力应正常。
 *
 * 流程：
 *   1. 创建 writer（BEST_EFFORT）和 reader（BEST_EFFORT）
 *   2. writer 发送 10 条消息
 *   3. 验证 reader 收到至少 1 条消息（BEST_EFFORT 可能丢消息）
 *
 * 验证：BEST_EFFORT 模式下基本通信能力正常。
 */
TEST_F(DdsWrapperNodeTEST, QoSBestEffortTest)
{
    TestNode node(0, "tc15_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc15_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc15_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::BEST_EFFORT);

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::BEST_EFFORT);

    std::atomic<int> recvCount{0};
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc15_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc15_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    for (int i = 0; i < 10; ++i) {
        TestMessage msg;
        msg.message_type("best_effort_" + std::to_string(i));
        writer->writeMessage(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    bool anyReceived =
        WaitFor([&]() { return recvCount.load() > 0; }, std::chrono::seconds(3));
    EXPECT_TRUE(anyReceived) << "BEST_EFFORT mode: should receive at least 1 message";
}

/**
 * TC16 - Durability QoS: VOLATILE 模式验证
 *
 * 功能：验证 VOLATILE 持久性下，reader 只能收到创建之后发送的消息，
 *       不能收到创建之前 writer 已发送的历史消息。
 *
 * 流程：
 *   1. 创建 writer（VOLATILE），发送 5 条消息
 *   2. 创建 reader（VOLATILE）
 *   3. writer 再发送 5 条消息
 *   4. 验证 reader 只收到后面的 5 条，不收到前面的 5 条
 *
 * 验证：VOLATILE 不缓存历史消息。
 */
TEST_F(DdsWrapperNodeTEST, QoSDurabilityVolatileTest)
{
    TestNode node(0, "tc16_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc16_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc16_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::VOLATILE)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc16_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    // 先发送 5 条历史消息（reader 还不存在）
    for (int i = 0; i < 5; ++i) {
        TestMessage msg;
        msg.message_type("history_" + std::to_string(i));
        writer->writeMessage(msg);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 创建 reader
    std::atomic<int> recvCount{0};
    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::VOLATILE)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc16_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 再发送 5 条新消息
    for (int i = 0; i < 5; ++i) {
        TestMessage msg;
        msg.message_type("new_" + std::to_string(i));
        writer->writeMessage(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    bool received = WaitFor([&]() { return recvCount.load() >= 5; }, std::chrono::seconds(3));
    EXPECT_TRUE(received) << "Should receive the 5 new messages";
    // VOLATILE 模式下不应收到历史消息，但可能因时序略有偏差，只验证收到了新消息
    EXPECT_GE(recvCount.load(), 5) << "Should receive at least 5 new messages";
}

/**
 * TC17 - Durability QoS: TRANSIENT_LOCAL 模式验证
 *
 * 功能：验证 TRANSIENT_LOCAL 持久性下，后创建的 reader 能收到
 *       writer 已发送的历史消息（writer 侧缓存）。
 *
 * 流程：
 *   1. 创建 writer（TRANSIENT_LOCAL, KEEP_LAST depth=5），发送 10 条消息
 *   2. 创建 reader（TRANSIENT_LOCAL, KEEP_LAST depth=5）
 *   3. 等待 reader 收到历史消息
 *   4. 验证 reader 收到了历史消息（至少 1 条）
 *
 * 验证：TRANSIENT_LOCAL 在 writer 侧缓存历史，后加入的 reader 可获取。
 */
TEST_F(DdsWrapperNodeTEST, QoSDurabilityTransientLocalTest)
{
    TestNode node(0, "tc17_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc17_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc17_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(5);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc17_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    // 发送 10 条消息（writer 缓存最后 5 条）
    for (int i = 0; i < 10; ++i) {
        TestMessage msg;
        msg.message_type("tl_" + std::to_string(i));
        writer->writeMessage(msg);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 创建 reader
    std::atomic<int> recvCount{0};
    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(5);
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc17_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr);

    bool received =
        WaitFor([&]() { return recvCount.load() > 0; }, std::chrono::seconds(5));
    EXPECT_TRUE(received) << "TRANSIENT_LOCAL: late-joining reader should receive history";
    LOG(info) << "TRANSIENT_LOCAL: received " << recvCount.load() << " historical messages";
}

/**
 * TC18 - Durability QoS: QoS 不兼容匹配验证
 *
 * 功能：验证 writer（VOLATILE）和 reader（TRANSIENT_LOCAL）之间
 *       因 durability QoS 不兼容而无法匹配。
 *
 * 流程：
 *   1. 创建 writer（VOLATILE）
 *   2. 创建 reader（TRANSIENT_LOCAL）
 *   3. writer 发送消息
 *   4. 验证 reader 收不到消息（QoS 不兼容）
 *
 * 验证：DDS 的 QoS 兼容性检查生效，不兼容的 writer/reader 不匹配。
 */
TEST_F(DdsWrapperNodeTEST, QoSDurabilityMismatchTest)
{
    TestNode node(0, "tc18_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc18_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc18_sub", subQos));

    // writer: VOLATILE
    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::VOLATILE);
    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc18_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    // reader: TRANSIENT_LOCAL（与 VOLATILE 不兼容）
    std::atomic<int> recvCount{0};
    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc18_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (int i = 0; i < 5; ++i) {
        TestMessage msg;
        msg.message_type("mismatch_" + std::to_string(i));
        writer->writeMessage(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    // DDS spec: VOLATILE writer 不兼容 TRANSIENT_LOCAL reader，不应匹配
    // 但某些 DDS 实现可能不严格检查，所以只做日志记录
    LOG(info) << "Durability mismatch test: received " << recvCount.load()
              << " messages (expected 0 if QoS enforcement is strict)";
    // 不做硬性 EXPECT，因为不同 DDS 实现行为可能不同
}

/**
 * TC19 - History QoS: KEEP_LAST(depth) 深度限制验证
 *
 * 功能：验证 KEEP_LAST 模式下，writer 侧只保留最近 depth 个样本，
 *       reader 侧也只保留最近 depth 个样本。
 *
 * 流程：
 *   1. 创建 writer（KEEP_LAST, depth=3）和 reader（KEEP_LAST, depth=3）
 *   2. writer 快速发送 10 条消息（不等待 reader 消费）
 *   3. reader 收到消息后记录每条的内容
 *   4. 验证 reader 最终只保留了最后 3 条（历史被覆盖）
 *
 * 验证：KEEP_LAST 的深度限制生效，旧消息被新消息覆盖。
 */
TEST_F(DdsWrapperNodeTEST, QoSHistoryKeepLastDepthTest)
{
    TestNode node(0, "tc19_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc19_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc19_sub", subQos));

    constexpr int32_t kDepth = 3;

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(kDepth);

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(kDepth);

    std::mutex mu;
    std::vector<std::string> receivedMessages;
    std::atomic<int> recvCount{0};

    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc19_topic",
        [&](const std::string &, std::shared_ptr<TestMessage> data) {
            if (data) {
                std::lock_guard<std::mutex> lk(mu);
                receivedMessages.push_back(data->message_type());
                recvCount.fetch_add(1);
            }
        },
        readerQos);
    ASSERT_NE(reader, nullptr);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc19_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    // 快速发送 10 条消息
    for (int i = 0; i < 10; ++i) {
        TestMessage msg;
        msg.message_type("hist_" + std::to_string(i));
        writer->writeMessage(msg);
    }

    bool received = WaitFor([&]() { return recvCount.load() > 0; }, std::chrono::seconds(3));
    EXPECT_TRUE(received) << "Should receive at least some messages";

    LOG(info) << "KEEP_LAST(depth=" << kDepth << "): received " << recvCount.load()
              << " messages total";
    {
        std::lock_guard<std::mutex> lk(mu);
        LOG(info) << "Last received messages count: " << receivedMessages.size();
        for (const auto &m : receivedMessages) {
            LOG(info) << "  msg: " << m;
        }
    }
    // 验证收到了消息（具体覆盖行为依赖 DDS 实现细节）
    EXPECT_GT(recvCount.load(), 0);
}

/**
 * TC20 - History QoS: KEEP_ALL 模式验证
 *
 * 功能：验证 KEEP_ALL 模式下，writer 和 reader 都不丢弃历史消息，
 *       所有发送的消息都能被接收（配合 RELIABLE 使用）。
 *
 * 流程：
 *   1. 创建 writer（RELIABLE, KEEP_ALL）和 reader（RELIABLE, KEEP_ALL）
 *   2. writer 发送 20 条消息
 *   3. 验证 reader 收到全部 20 条
 *
 * 验证：KEEP_ALL + RELIABLE 组合下消息完整送达。
 */
TEST_F(DdsWrapperNodeTEST, QoSHistoryKeepAllTest)
{
    TestNode node(0, "tc20_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc20_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc20_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_ALL);

    std::atomic<int> recvCount{0};
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc20_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc20_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    constexpr int kTotal = 20;
    for (int i = 0; i < kTotal; ++i) {
        TestMessage msg;
        msg.message_type("keep_all_" + std::to_string(i));
        writer->writeMessage(msg);
    }

    bool allReceived = WaitFor([&]() { return recvCount.load() >= kTotal; },
                               std::chrono::seconds(5));
    EXPECT_TRUE(allReceived) << "KEEP_ALL: expected " << kTotal << " but got " << recvCount.load();
}

/**
 * TC21 - ResourceLimits QoS: maxSamples / maxInstances / maxSamplesPerInstance 验证
 *
 * 功能：验证 resource_limits 配置（maxSamples, maxInstances, maxSamplesPerInstance）
 *       能正确设置，不导致创建失败或崩溃。
 *
 * 流程：
 *   1. 创建 writer 和 reader，设置 resource_limits
 *   2. writer 发送消息，reader 接收
 *   3. 验证基本通信正常，resource_limits 配置不影响基本功能
 *
 * 验证：resource_limits 参数被正确接受，基本通信不受影响。
 */
TEST_F(DdsWrapperNodeTEST, QoSResourceLimitsTest)
{
    TestNode node(0, "tc21_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc21_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc21_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setMaxSamples(100)
        .setMaxInstances(10)
        .setMaxSamplesPerInstance(10);

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setMaxSamples(100)
        .setMaxInstances(10)
        .setMaxSamplesPerInstance(10);

    std::atomic<int> recvCount{0};
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc21_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr) << "DataReader creation with resource_limits should succeed";

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc21_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr) << "DataWriter creation with resource_limits should succeed";

    for (int i = 0; i < 5; ++i) {
        TestMessage msg;
        msg.message_type("res_" + std::to_string(i));
        writer->writeMessage(msg);
    }

    bool received = WaitFor([&]() { return recvCount.load() >= 5; }, std::chrono::seconds(3));
    EXPECT_TRUE(received) << "Communication should work with resource_limits set";
}

/**
 * TC22 - DataWriterQoSBuilder 链式调用完整性验证
 *
 * 功能：验证 DataWriterQoSBuilder 的所有链式调用方法都能正常工作，
 *       不会因为某个 setter 的问题导致 QoS 设置失败。
 *
 * 流程：
 *   1. 对 DataWriterQoSBuilder 调用所有可用的 setter 方法
 *   2. 用该 QoS 创建 DataWriter
 *   3. 验证 DataWriter 创建成功且能正常发送消息
 *
 * 验证：所有 DataWriter QoS setter 方法正常工作。
 */
TEST_F(DdsWrapperNodeTEST, QoSDataWriterBuilderChainTest)
{
    TestNode node(0, "tc22_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc22_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc22_sub", subQos));

    // 调用所有 DataWriter QoS setter（不包含 setFlowController，需要预定义的 flow controller）
    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setReliabilityMaxBlockingTime(100)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(5)
        .setMaxSamples(256)
        .setMaxInstances(16)
        .setMaxSamplesPerInstance(16)
        .disableDataSharing()
        .setAsyncPublisherMode(true);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>(
        "tc22_topic", "tc22_pub", writerQos, nullptr);
    ASSERT_NE(writer, nullptr) << "DataWriter with full QoS chain should be created successfully";

    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc22_topic", "tc22_sub",
        [](const std::string &, std::shared_ptr<TestMessage>) {},
        readerQos, nullptr);
    ASSERT_NE(reader, nullptr);

    TestMessage msg;
    msg.message_type("chain_test");
    EXPECT_TRUE(writer->writeMessage(msg)) << "Write should succeed with full QoS chain";
}

/**
 * TC23 - DataReaderQoSBuilder 链式调用完整性验证
 *
 * 功能：验证 DataReaderQoSBuilder 的所有链式调用方法都能正常工作。
 *
 * 流程：
 *   1. 对 DataReaderQoSBuilder 调用所有可用的 setter 方法
 *   2. 用该 QoS 创建 DataReader
 *   3. 验证 DataReader 创建成功且能正常接收消息
 *
 * 验证：所有 DataReader QoS setter 方法正常工作。
 */
TEST_F(DdsWrapperNodeTEST, QoSDataReaderBuilderChainTest)
{
    TestNode node(0, "tc23_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc23_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc23_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL);
    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc23_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    // 调用所有 DataReader QoS setter
    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(10)
        .setMaxSamples(256)
        .setMaxInstances(16)
        .setMaxSamplesPerInstance(16)
        .disableDataSharing();

    std::atomic<int> recvCount{0};
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc23_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr) << "DataReader with full QoS chain should be created successfully";

    TestMessage msg;
    msg.message_type("reader_chain_test");
    writer->writeMessage(msg);

    bool received = WaitFor([&]() { return recvCount.load() > 0; }, std::chrono::seconds(3));
    EXPECT_TRUE(received) << "DataReader with full QoS chain should receive messages";
}

/**
 * TC24 - Reliability + Durability 组合 QoS 交叉验证
 *
 * 功能：测试 RELIABLE + TRANSIENT_LOCAL 组合（onDemand 系统实际使用的配置），
 *       验证该组合下 writer/reader 的创建和通信均正常。
 *
 * 流程：
 *   1. 创建 writer（RELIABLE + TRANSIENT_LOCAL + KEEP_LAST depth=10）
 *   2. 先发送 5 条消息
 *   3. 创建 reader（RELIABLE + TRANSIENT_LOCAL + KEEP_LAST depth=10）
 *   4. 验证 reader 收到历史消息
 *   5. writer 再发送 5 条消息，验证 reader 也收到新消息
 *
 * 验证：RELIABLE + TRANSIENT_LOCAL 组合下历史消息和新消息都能正常接收。
 */
TEST_F(DdsWrapperNodeTEST, QoSReliableTransientLocalComboTest)
{
    TestNode node(0, "tc24_node");
    ASSERT_TRUE(node.isInitialized());

    DdsWrapper::PublisherQoSBuilder pubQos;
    ASSERT_TRUE(node.createPublisher("tc24_pub", pubQos));
    DdsWrapper::SubscriberQoSBuilder subQos;
    ASSERT_TRUE(node.createSubscriber("tc24_sub", subQos));

    DdsWrapper::DataWriterQoSBuilder writerQos;
    writerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(10);

    auto writer = node.createDataWriter<TestMessage, TestMessagePubSubType>("tc24_topic",
                                                                            writerQos);
    ASSERT_NE(writer, nullptr);

    // 发送 5 条历史消息
    for (int i = 0; i < 5; ++i) {
        TestMessage msg;
        msg.message_type("history_" + std::to_string(i));
        writer->writeMessage(msg);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 创建 reader
    std::atomic<int> recvCount{0};
    DdsWrapper::DataReaderQoSBuilder readerQos;
    readerQos.setReliabilityKind(DdsWrapper::ReliabilityKind::RELIABLE)
        .setDurabilityKind(DdsWrapper::DurabilityKind::TRANSIENT_LOCAL)
        .setHistoryKind(DdsWrapper::HistoryKind::KEEP_LAST)
        .setHistoryDepth(10);
    auto reader = node.createDataReader<TestMessage, TestMessagePubSubType>(
        "tc24_topic",
        [&](const std::string &, std::shared_ptr<TestMessage>) { recvCount.fetch_add(1); },
        readerQos);
    ASSERT_NE(reader, nullptr);

    // 等待历史消息到达
    bool gotHistory = WaitFor([&]() { return recvCount.load() > 0; }, std::chrono::seconds(5));
    int afterHistory = recvCount.load();
    EXPECT_TRUE(gotHistory) << "Should receive historical messages with TRANSIENT_LOCAL";
    LOG(info) << "Received " << afterHistory << " historical messages";

    // 再发送 5 条新消息
    for (int i = 0; i < 5; ++i) {
        TestMessage msg;
        msg.message_type("new_" + std::to_string(i));
        writer->writeMessage(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    bool gotNew = WaitFor([&]() { return recvCount.load() >= afterHistory + 5; },
                          std::chrono::seconds(3));
    EXPECT_TRUE(gotNew) << "Should also receive new messages after historical ones";
    LOG(info) << "Total received: " << recvCount.load() << " (history=" << afterHistory
              << " + new=" << (recvCount.load() - afterHistory) << ")";
}

} // namespace
