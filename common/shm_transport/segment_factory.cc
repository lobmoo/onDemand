/**
 * @file segment_factory.cc
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
#include "segment_factory.h"
#include "posix_segment.h"
#include "common.h"
#include "log/logger.h"

namespace shm_module {


std::shared_ptr<Segment> SegmentFactory::CreateSegment(uint64_t id) {
  LOG(info) << "Creating POSIX segment, channel_id: " << id;
  return std::make_shared<PosixSegment>(id);
}

}  // namespace shm_module
