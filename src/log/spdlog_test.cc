#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> // 控制台输出
#include <spdlog/sinks/basic_file_sink.h>   // 文件输出
#include <chrono>

using namespace std::chrono;

#define LOG_INFO(logger, message) logger->info("({}:{}) {}", __FILE__, __LINE__, message)

class SpdlogPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建同时输出到控制台和文件的日志器
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logfile.txt", true);

        // 设置日志格式
        std::string pattern(R"([%Y-%m-%d %H:%M:%S.%e|%t] [%l] %v)");
        console_sink->set_pattern(pattern);
        file_sink->set_pattern(pattern);

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        logger = std::make_shared<spdlog::logger>("multi_sink", begin(sinks), end(sinks));
        spdlog::register_logger(logger);
    }

    void TearDown() override {
        // 清理
        spdlog::drop_all();
    }

    std::shared_ptr<spdlog::logger> logger;
};

TEST_F(SpdlogPerformanceTest, LogPerformanceSync) {
    auto start = high_resolution_clock::now();

    for(int i = 0; i < 10000; ++i){
        LOG_INFO(logger, "Performance test log message " + std::to_string(i) + " zheyangzi");
    }

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

    std::cout << "Time taken by sync logging function: " << duration.count() << " milliseconds" << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}