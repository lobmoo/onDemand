/**
 * @file block.h
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

#ifndef SHM_MODULE_BLOCK_H_
#define SHM_MODULE_BLOCK_H_

#include <atomic>
#include <cstdint>

namespace shm_module
{

class Block
{
    friend class Segment;

public:
    Block();
    virtual ~Block();

    uint64_t msg_size() const { return msg_size_; }
    void set_msg_size(uint64_t msg_size) { msg_size_ = msg_size; }

    uint64_t msg_info_size() const { return msg_info_size_; }
    void set_msg_info_size(uint64_t msg_info_size) { msg_info_size_ = msg_info_size; }

    static const int32_t kRWLockFree;
    static const int32_t kWriteExclusive;
    static const int32_t kMaxTryLockTimes;

private:
    bool TryLockForWrite();
    bool TryLockForRead();
    void ReleaseWriteLock();
    void ReleaseReadLock();

    std::atomic<int32_t> lock_num_ = {0};

    uint64_t msg_size_;
    uint64_t msg_info_size_;
};

} // namespace shm_module

#endif // SHM_MODULE_BLOCK_H_
