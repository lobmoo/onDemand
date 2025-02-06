// #include <unistd.h>
// #include <iostream>
// #include <thread>
// #include <vector>
// #include "logger.h"

// void logMessages(int threadId, int numMessages) {
//     for (int i = 0; i < numMessages; ++i) {
//         LOG(trace) << "Thread " << threadId << " - Trace message " << i;
//         LOG(debug) << "Thread " << threadId << " - Debug message " << i;
//         LOG(info)<< "Thread " << threadId << " - Info message " << i;
//         LOG(warning)<< "Thread " << threadId << " - Warn message " << i;
//         LOG(error) << "Thread " << threadId << " - Error message " << i;
//         LOG(fatal) << "Thread " << threadId << " - fatal message " << i;
//     }
// }

// int main() {
//     // 初始化日志记录器
//     if (!Logger::Instance().Init("myapp.log", Logger::both, 0, 5, 3, false)) {
//         std::cerr << "Failed to initialize logger" << std::endl;
//         return 1;
//     }

//     // 设置控制台和文件的日志级别
//     Logger::Instance().setConsoleLogLevel(Logger::info);
//     Logger::Instance().setFileLogLevel(Logger::info);

//     const int numThreads = 20;
//     const int numMessages = 1000;

//     std::vector<std::thread> threads;
//     for (int i = 0; i < numThreads; ++i) {
//         threads.emplace_back(logMessages, i, numMessages);
//     }

//     for (auto& thread : threads) {
//         thread.join();
//     }

//     std::cout << "Logging completed." << std::endl;
//     return 0;
// }

int main()
{

    return 0;
}