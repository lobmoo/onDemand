/**
 * @file shm_conf.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2026-01-26
 * 
 * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2026-01-26     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */

#ifndef SHM_MODULE_SHM_CONF_H_
#define SHM_MODULE_SHM_CONF_H_

#include <cstdint>

namespace shm_module
{

class ShmConf
{
public:
    ShmConf();
    explicit ShmConf(const uint64_t &real_msg_size);
    virtual ~ShmConf();

    void Update(const uint64_t &real_msg_size);

    const uint64_t &ceiling_msg_size() { return ceiling_msg_size_; }
    const uint64_t &block_buf_size() { return block_buf_size_; }
    const uint32_t &block_num() { return block_num_; }
    const uint64_t &managed_shm_size() { return managed_shm_size_; }

private:
    uint64_t GetCeilingMessageSize(const uint64_t &real_msg_size);
    uint64_t GetBlockBufSize(const uint64_t &ceiling_msg_size);
    uint32_t GetBlockNum(const uint64_t &ceiling_msg_size);

    uint64_t ceiling_msg_size_;
    uint64_t block_buf_size_;
    uint32_t block_num_;
    uint64_t managed_shm_size_;

    // Extra size, Byte
    static const uint64_t EXTRA_SIZE;
    // State size, Byte
    static const uint64_t STATE_SIZE;
    // Block size, Byte
    static const uint64_t BLOCK_SIZE;
    // Message info size, Byte (预留元数据空间)
    static const uint64_t MESSAGE_INFO_SIZE;

    //For 0-8B
    static const uint32_t BLOCK_NUM_8B;
    static const uint64_t MESSAGE_SIZE_8B;

    //ADD

    //For 0-8KB
    static const uint32_t BLOCK_NUM_8K;
    static const uint64_t MESSAGE_SIZE_8K;
    // For message 0-16KB
    static const uint32_t BLOCK_NUM_16K;
    static const uint64_t MESSAGE_SIZE_16K;
    // For message 16K-128K
    static const uint32_t BLOCK_NUM_128K;
    static const uint64_t MESSAGE_SIZE_128K;
    // For message 128K-1M
    static const uint32_t BLOCK_NUM_1M;
    static const uint64_t MESSAGE_SIZE_1M;
    // For message 1M-8M
    static const uint32_t BLOCK_NUM_8M;
    static const uint64_t MESSAGE_SIZE_8M;
    // For message 8M-16M
    static const uint32_t BLOCK_NUM_16M;
    static const uint64_t MESSAGE_SIZE_16M;
    // For message > 16M
    static const uint32_t BLOCK_NUM_MORE;
    static const uint64_t MESSAGE_SIZE_MORE;
};

} // namespace shm_module

#endif // SHM_MODULE_SHM_CONF_H_
