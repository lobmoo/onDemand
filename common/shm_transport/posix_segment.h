/**
 * @file posix_segment.h
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

#ifndef SHM_MODULE_POSIX_SEGMENT_H_
#define SHM_MODULE_POSIX_SEGMENT_H_

#include <string>
#include "segment.h"

namespace shm_module {

class PosixSegment : public Segment {
 public:
  explicit PosixSegment(uint64_t channel_id);
  virtual ~PosixSegment();

  static const char* Type() { return "posix"; }

 protected:
  bool OpenOrCreate() override;
  bool OpenOnly() override;
  bool Remove() override;
  void Reset() override;

 private:
  std::string shm_name_;
};

}  // namespace shm_module

#endif  // SHM_MODULE_POSIX_SEGMENT_H_
