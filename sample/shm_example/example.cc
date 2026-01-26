/******************************************************************************
 * Standalone Shared Memory Module - Example
 * 展示如何使用共享内存模块进行进程间二进制数据传输
 *****************************************************************************/

#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <memory>
#include "shm_transport/posix_segment.h"
#include "shm_transport/segment_factory.h"

using namespace shm_module;

// 写进程示例
void WriterProcess(uint64_t channel_id)
{
    std::cout << "[Writer] Starting writer process..." << std::endl;

    // 创建共享内存段
    auto segment = std::make_shared<PosixSegment>(channel_id);

    // 准备要写入的数据
    const char *message = "Hello from shared memory!";
    size_t msg_size = strlen(message) + 1;

    WritableBlock wb;
    if (segment->AcquireBlockToWrite(msg_size, &wb)) {
        // 写入数据
        memcpy(wb.buf, message, msg_size);
        wb.block->set_msg_size(msg_size);

        segment->ReleaseWrittenBlock(wb);
        std::cout << "[Writer] Successfully wrote: " << message << std::endl;
        std::cout << "[Writer] Block index: " << wb.index << std::endl;
    } else {
        std::cerr << "[Writer] Failed to acquire writable block" << std::endl;
    }
}

// 读进程示例
void ReaderProcess(uint64_t channel_id, uint32_t block_index)
{
    std::cout << "[Reader] Starting reader process..." << std::endl;

    // 打开现有的共享内存段
    auto segment = std::make_shared<PosixSegment>(channel_id);

    ReadableBlock rb;
    rb.index = block_index;

    if (segment->AcquireBlockToRead(&rb)) {
        // 读取数据
        size_t msg_size = rb.block->msg_size();
        char *data = new char[msg_size];
        memcpy(data, rb.buf, msg_size);

        std::cout << "[Reader] Successfully read: " << data << std::endl;
        std::cout << "[Reader] Message size: " << msg_size << " bytes" << std::endl;

        delete[] data;
        segment->ReleaseReadBlock(rb);
    } else {
        std::cerr << "[Reader] Failed to acquire readable block" << std::endl;
    }
}

// 二进制数据示例
void BinaryDataExample(uint64_t channel_id)
{
    std::cout << "\n[Binary Example] Writing binary data..." << std::endl;

    auto segment = std::make_shared<PosixSegment>(channel_id);

    // 准备二进制数据 (例如：图像、传感器数据等)
    struct SensorData {
        uint64_t timestamp;
        double temperature;
        double pressure;
        uint8_t raw_data[256];
    };

    SensorData sensor;
    sensor.timestamp = 1234567890;
    sensor.temperature = 25.5;
    sensor.pressure = 101.3;
    for (int i = 0; i < 256; i++) {
        sensor.raw_data[i] = i;
    }

    WritableBlock wb;
    if (segment->AcquireBlockToWrite(sizeof(SensorData), &wb)) {
        // 写入二进制结构体
        memcpy(wb.buf, &sensor, sizeof(SensorData));
        wb.block->set_msg_size(sizeof(SensorData));

        segment->ReleaseWrittenBlock(wb);
        std::cout << "[Binary Example] Wrote sensor data to block: " << wb.index << std::endl;

        // 读取验证
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ReadableBlock rb;
        rb.index = wb.index;
        if (segment->AcquireBlockToRead(&rb)) {
            SensorData *read_sensor = reinterpret_cast<SensorData *>(rb.buf);
            std::cout << "[Binary Example] Read sensor data:" << std::endl;
            std::cout << "  Timestamp: " << read_sensor->timestamp << std::endl;
            std::cout << "  Temperature: " << read_sensor->temperature << " °C" << std::endl;
            std::cout << "  Pressure: " << read_sensor->pressure << " kPa" << std::endl;
            std::cout << "  Raw data[0]: " << (int)read_sensor->raw_data[0] << std::endl;
            std::cout << "  Raw data[255]: " << (int)read_sensor->raw_data[255] << std::endl;

            segment->ReleaseReadBlock(rb);
        }
    }
}

//工厂模式示例
void FactoryModeExampleWrite(uint64_t channel_id)
{
    std::cout << "\n[Factory Mode Example] Using factory to create segment..." << std::endl;

    // 使用工厂方法创建共享内存段
    auto segment = SegmentFactory::CreateSegment(channel_id);
    if (!segment) {
        std::cerr << "[Factory Mode Example] Failed to create segment via factory" << std::endl;
        return;
    }

    // 准备要写入的数据
    const char *message = "Hello from shared memory!";
    size_t msg_size = strlen(message) + 1;

    WritableBlock wb;
    if (segment->AcquireBlockToWrite(msg_size, &wb)) {
        // 写入数据
        memcpy(wb.buf, message, msg_size);
        wb.block->set_msg_size(msg_size);

        segment->ReleaseWrittenBlock(wb);
        std::cout << "[Writer] Successfully wrote: " << message << std::endl;
        std::cout << "[Writer] Block index: " << wb.index << std::endl;
    } else {
        std::cerr << "[Writer] Failed to acquire writable block" << std::endl;
    }
}

void FactoryModeExampleRead(uint64_t channel_id)
{
    std::cout << "\n[Factory Mode Example] Using factory to open segment..." << std::endl;

    // 使用工厂方法打开现有的共享内存段
    auto segment = SegmentFactory::CreateSegment(channel_id);
    if (!segment) {
        std::cerr << "[Factory Mode Example] Failed to open segment via factory" << std::endl;
        return;
    }

    ReadableBlock rb;
    rb.index = 0; // 假设读取第一个块

    if (segment->AcquireBlockToRead(&rb)) {
        // 读取数据
        size_t msg_size = rb.block->msg_size();
        char *data = new char[msg_size];
        memcpy(data, rb.buf, msg_size);

        std::cout << "[Reader] Successfully read: " << data << std::endl;
        std::cout << "[Reader] Message size: " << msg_size << " bytes" << std::endl;

        delete[] data;
        segment->ReleaseReadBlock(rb);
    } else {
        std::cerr << "[Reader] Failed to acquire readable block" << std::endl;
    }
}



int main()
{
    std::cout << "=== Standalone Shared Memory Module Example ===" << std::endl;

    uint64_t channel_id = 12345; // 共享内存的唯一标识

    // 示例1: 简单的字符串传输
    std::cout << "\n--- Example 1: String Transfer ---" << std::endl;
    WriterProcess(channel_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ReaderProcess(channel_id, 0); // 读取第一个写入的块

    // 示例2: 二进制数据传输
    std::cout << "\n--- Example 2: Binary Data Transfer ---" << std::endl;
    BinaryDataExample(channel_id);

    // 示例3: 多次写入
    std::cout << "\n--- Example 3: Multiple Writes ---" << std::endl;
    auto segment = std::make_shared<PosixSegment>(channel_id);
    for (int i = 0; i < 5; i++) {
        WritableBlock wb;
        std::string msg = "Message " + std::to_string(i);
        if (segment->AcquireBlockToWrite(msg.size() + 1, &wb)) {
            memcpy(wb.buf, msg.c_str(), msg.size() + 1);
            wb.block->set_msg_size(msg.size() + 1);
            segment->ReleaseWrittenBlock(wb);
            std::cout << "[Multi-Write] Wrote: " << msg << " to block " << wb.index << std::endl;
        }
    }

    //示例 Factory 模式
    std::cout << "\n--- Example 4: Factory Mode ---" << std::endl;
    FactoryModeExampleWrite(channel_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    FactoryModeExampleRead(channel_id);

    std::cout << "\n=== Example completed ===" << std::endl;
    std::cout << "Note: The shared memory segment remains until system reboot or manual cleanup."
              << std::endl;
    std::cout << "Use 'ls /dev/shm/' to see the shared memory files." << std::endl;

    return 0;
}
