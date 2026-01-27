/**
 * @file segment_factory.h
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
#ifndef SHM_MODULE_SEGMENT_FACTORY_H_
#define SHM_MODULE_SEGMENT_FACTORY_H_

#include <memory>
#include "segment.h"

namespace shm_module
{

class SegmentFactory
{
public:
    static std::shared_ptr<Segment> CreateSegment(uint64_t id);
};

} // namespace shm_module

#endif // SHM_MODULE_SEGMENT_FACTORY_H_
