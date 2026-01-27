/**
 * @file segment.h
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

#ifndef SHM_MODULE_SEGMENT_H_
#define SHM_MODULE_SEGMENT_H_

#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdint>

#include "block.h"
#include "shm_conf.h"
#include "state.h"

namespace shm_module
{

class Segment;
using SegmentPtr = std::shared_ptr<Segment>;

struct WritableBlock {
    uint32_t index = 0;
    Block *block = nullptr;
    uint8_t *buf = nullptr;
};
using ReadableBlock = WritableBlock;

class Segment
{
public:
    explicit Segment(uint64_t channel_id);
    virtual ~Segment() {}

    bool AcquireBlockToWrite(std::size_t msg_size, WritableBlock *writable_block);
    void ReleaseWrittenBlock(const WritableBlock &writable_block);

    bool AcquireBlockToRead(ReadableBlock *readable_block);
    void ReleaseReadBlock(const ReadableBlock &readable_block);

protected:
    virtual bool Destroy();
    virtual void Reset() = 0;
    virtual bool Remove() = 0;
    virtual bool OpenOnly() = 0;
    virtual bool OpenOrCreate() = 0;

    bool init_;
    ShmConf conf_;
    uint64_t channel_id_;

    State *state_;
    Block *blocks_;
    void *managed_shm_;
    std::mutex block_buf_lock_;
    std::unordered_map<uint32_t, uint8_t *> block_buf_addrs_;

private:
    bool Remap();
    bool Recreate(const uint64_t &msg_size);
    uint32_t GetNextWritableBlockIndex();
};

} // namespace shm_module

#endif // SHM_MODULE_SEGMENT_H_
