/**
 * @file segment.cc
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

#include "segment.h"
#include "common.h"

namespace shm_module {

Segment::Segment(uint64_t channel_id)
    : init_(false),
      conf_(),
      channel_id_(channel_id),
      state_(nullptr),
      blocks_(nullptr),
      managed_shm_(nullptr),
      block_buf_lock_(),
      block_buf_addrs_() {}

bool Segment::AcquireBlockToWrite(std::size_t msg_size,
                                  WritableBlock* writable_block) {
  RETURN_VAL_IF_NULL(writable_block, false);
  if (!init_ && !OpenOrCreate()) {
    SHM_ERROR("Create shm failed, can't write now");
    return false;
  }

  bool result = true;
  if (state_->need_remap()) {
    result = Remap();
  }

  if (msg_size > conf_.ceiling_msg_size()) {
    SHM_INFO("msg_size: " << msg_size 
             << " larger than current shm_buffer_size: "
             << conf_.ceiling_msg_size() << ", need recreate");
    result = Recreate(msg_size);
  }

  if (!result) {
    SHM_ERROR("Segment update failed");
    return false;
  }

  uint32_t index = GetNextWritableBlockIndex();
  writable_block->index = index;
  writable_block->block = &blocks_[index];
  writable_block->buf = block_buf_addrs_[index];
  return true;
}

void Segment::ReleaseWrittenBlock(const WritableBlock& writable_block) {
  auto index = writable_block.index;
  if (index >= conf_.block_num()) {
    return;
  }
  blocks_[index].ReleaseWriteLock();
}

bool Segment::AcquireBlockToRead(ReadableBlock* readable_block) {
  RETURN_VAL_IF_NULL(readable_block, false);
  if (!init_ && !OpenOnly()) {
    SHM_ERROR("Failed to open shared memory, can't read now");
    return false;
  }

  auto index = readable_block->index;
  if (index >= conf_.block_num()) {
    SHM_ERROR("Invalid block_index: " << index);
    return false;
  }

  bool result = true;
  if (state_->need_remap()) {
    result = Remap();
  }

  if (!result) {
    SHM_ERROR("Segment update failed");
    return false;
  }

  if (!blocks_[index].TryLockForRead()) {
    return false;
  }
  readable_block->block = blocks_ + index;
  readable_block->buf = block_buf_addrs_[index];
  return true;
}

void Segment::ReleaseReadBlock(const ReadableBlock& readable_block) {
  auto index = readable_block.index;
  if (index >= conf_.block_num()) {
    return;
  }
  blocks_[index].ReleaseReadLock();
}

bool Segment::Destroy() {
  if (!init_) {
    return true;
  }
  init_ = false;

  try {
    state_->DecreaseReferenceCounts();
    uint32_t reference_counts = state_->reference_counts();
    if (reference_counts == 0) {
      return Remove();
    }
  } catch (...) {
    SHM_ERROR("Exception in Destroy");
    return false;
  }
  SHM_DEBUG("Destroy complete");
  return true;
}

bool Segment::Remap() {
  init_ = false;
  SHM_DEBUG("Before reset");
  Reset();
  SHM_DEBUG("After reset");
  return OpenOnly();
}

bool Segment::Recreate(const uint64_t& msg_size) {
  if (!Destroy()) {
    SHM_ERROR("Destroy failed");
    return false;
  }

  Reset();
  state_ = nullptr;
  blocks_ = nullptr;
  managed_shm_ = nullptr;
  {
    std::lock_guard<std::mutex> lock(block_buf_lock_);
    block_buf_addrs_.clear();
  }

  conf_.Update(msg_size);
  return OpenOrCreate();
}

uint32_t Segment::GetNextWritableBlockIndex() {
  uint32_t try_idx = 0;
  while (try_idx < conf_.block_num()) {
    uint32_t index = state_->FetchAddSeq(1) % conf_.block_num();
    if (blocks_[index].TryLockForWrite()) {
      return index;
    }
    ++try_idx;
  }
  SHM_ERROR("Failed to get writable block");
  return 0;
}

}  // namespace shm_module
