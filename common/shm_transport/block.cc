/**
 * @file block.cc
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
#include "block.h"
#include "common.h"

namespace shm_module {

const int32_t Block::kRWLockFree = 0;
const int32_t Block::kWriteExclusive = -1;
const int32_t Block::kMaxTryLockTimes = 5;

Block::Block() : msg_size_(0), msg_info_size_(0) {}

Block::~Block() {}

bool Block::TryLockForWrite() {
  int32_t rw_lock_free = kRWLockFree;
  if (!lock_num_.compare_exchange_weak(rw_lock_free, kWriteExclusive,
                                       std::memory_order_acq_rel,
                                       std::memory_order_relaxed)) {
    SHM_DEBUG("Lock num: " << lock_num_.load());
    return false;
  }
  return true;
}

bool Block::TryLockForRead() {
  int32_t lock_num = lock_num_.load();
  if (lock_num < kRWLockFree) {
    SHM_INFO("Block is being written");
    return false;
  }

  int32_t try_times = 0;
  while (!lock_num_.compare_exchange_weak(lock_num, lock_num + 1,
                                          std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
    ++try_times;
    if (try_times == kMaxTryLockTimes) {
      SHM_INFO("Failed to add read lock num, current num: " << lock_num);
      return false;
    }

    lock_num = lock_num_.load();
    if (lock_num < kRWLockFree) {
      SHM_INFO("Block is being written");
      return false;
    }
  }

  return true;
}

void Block::ReleaseWriteLock() { lock_num_.fetch_add(1); }

void Block::ReleaseReadLock() { lock_num_.fetch_sub(1); }

}  // namespace shm_module
