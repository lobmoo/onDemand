/**
 * @file posix_segment.cc
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

#include "posix_segment.h"
#include "common.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace shm_module {

PosixSegment::PosixSegment(uint64_t channel_id) : Segment(channel_id) {
  shm_name_ = std::to_string(channel_id);
}

PosixSegment::~PosixSegment() { Destroy(); }

bool PosixSegment::OpenOrCreate() {
  if (init_) {
    return true;
  }

  // create shm
  int fd = shm_open(shm_name_.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
  if (fd == -1) {
    if (EEXIST == errno) {
      SHM_DEBUG("Shm already exists, open only");
      return OpenOnly();
    } else {
      SHM_ERROR("Create shm failed: " << strerror(errno));
      return false;
    }
  }

  // truncate
  if (ftruncate(fd, conf_.managed_shm_size()) == -1) {
    SHM_ERROR("Truncate failed: " << strerror(errno));
    close(fd);
    shm_unlink(shm_name_.c_str());
    return false;
  }

  // mmap
  managed_shm_ = mmap(nullptr, conf_.managed_shm_size(), PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
  if (managed_shm_ == MAP_FAILED) {
    SHM_ERROR("Attach shm failed: " << strerror(errno));
    close(fd);
    shm_unlink(shm_name_.c_str());
    return false;
  }

  close(fd);

  // create field state_
  state_ = new (managed_shm_) State(conf_.ceiling_msg_size());
  if (state_ == nullptr) {
    SHM_ERROR("Create state failed");
    munmap(managed_shm_, conf_.managed_shm_size());
    managed_shm_ = nullptr;
    shm_unlink(shm_name_.c_str());
    return false;
  }

  conf_.Update(state_->ceiling_msg_size());

  // create field blocks_
  blocks_ = new (static_cast<char*>(managed_shm_) + sizeof(State))
      Block[conf_.block_num()];
  if (blocks_ == nullptr) {
    SHM_ERROR("Create blocks failed");
    state_->~State();
    state_ = nullptr;
    munmap(managed_shm_, conf_.managed_shm_size());
    managed_shm_ = nullptr;
    shm_unlink(shm_name_.c_str());
    return false;
  }

  // create block buf
  uint32_t i = 0;
  for (; i < conf_.block_num(); ++i) {
    uint8_t* addr =
        new (static_cast<char*>(managed_shm_) + sizeof(State) +
             conf_.block_num() * sizeof(Block) + i * conf_.block_buf_size())
            uint8_t[conf_.block_buf_size()];

    if (addr == nullptr) {
      break;
    }

    std::lock_guard<std::mutex> lock(block_buf_lock_);
    block_buf_addrs_[i] = addr;
  }

  if (i != conf_.block_num()) {
    SHM_ERROR("Create block buf failed");
    state_->~State();
    state_ = nullptr;
    blocks_ = nullptr;
    {
      std::lock_guard<std::mutex> lock(block_buf_lock_);
      block_buf_addrs_.clear();
    }
    munmap(managed_shm_, conf_.managed_shm_size());
    managed_shm_ = nullptr;
    shm_unlink(shm_name_.c_str());
    return false;
  }

  state_->IncreaseReferenceCounts();
  init_ = true;
  return true;
}

bool PosixSegment::OpenOnly() {
  if (init_) {
    return true;
  }

  // open shm
  int fd = shm_open(shm_name_.c_str(), O_RDWR, 0644);
  if (fd == -1) {
    SHM_ERROR("Open shm failed: " << strerror(errno));
    return false;
  }

  struct stat file_attr;
  if (fstat(fd, &file_attr) == -1) {
    SHM_ERROR("Fstat failed: " << strerror(errno));
    close(fd);
    return false;
  }

  // mmap
  managed_shm_ = mmap(nullptr, file_attr.st_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
  if (managed_shm_ == MAP_FAILED) {
    SHM_ERROR("Attach shm failed: " << strerror(errno));
    close(fd);
    return false;
  }

  close(fd);

  // get field state_
  state_ = reinterpret_cast<State*>(managed_shm_);
  if (state_ == nullptr) {
    SHM_ERROR("Get state failed");
    munmap(managed_shm_, file_attr.st_size);
    managed_shm_ = nullptr;
    return false;
  }

  conf_.Update(state_->ceiling_msg_size());

  // get field blocks_
  blocks_ = reinterpret_cast<Block*>(static_cast<char*>(managed_shm_) +
                                     sizeof(State));
  if (blocks_ == nullptr) {
    SHM_ERROR("Get blocks failed");
    state_ = nullptr;
    munmap(managed_shm_, conf_.managed_shm_size());
    managed_shm_ = nullptr;
    return false;
  }

  // get block buf
  uint32_t i = 0;
  for (; i < conf_.block_num(); ++i) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(
        static_cast<char*>(managed_shm_) + sizeof(State) +
        conf_.block_num() * sizeof(Block) + i * conf_.block_buf_size());

    std::lock_guard<std::mutex> lock(block_buf_lock_);
    block_buf_addrs_[i] = addr;
  }

  if (i != conf_.block_num()) {
    SHM_ERROR("Open only failed");
    state_->~State();
    state_ = nullptr;
    blocks_ = nullptr;
    {
      std::lock_guard<std::mutex> lock(block_buf_lock_);
      block_buf_addrs_.clear();
    }
    munmap(managed_shm_, conf_.managed_shm_size());
    managed_shm_ = nullptr;
    shm_unlink(shm_name_.c_str());
    return false;
  }

  state_->IncreaseReferenceCounts();
  init_ = true;
  SHM_DEBUG("Open only success");
  return true;
}

bool PosixSegment::Remove() {
  if (shm_unlink(shm_name_.c_str()) == -1) {
    SHM_ERROR("Remove failed: " << strerror(errno));
    return false;
  }
  SHM_DEBUG("Remove success");
  return true;
}

void PosixSegment::Reset() {
  if (managed_shm_ != nullptr) {
    munmap(managed_shm_, conf_.managed_shm_size());
    managed_shm_ = nullptr;
  }

  state_ = nullptr;
  blocks_ = nullptr;
  {
    std::lock_guard<std::mutex> lock(block_buf_lock_);
    block_buf_addrs_.clear();
  }
  init_ = false;
}

}  // namespace shm_module
