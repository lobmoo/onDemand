
/**
 * @file state.cc
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

#include "state.h"

namespace shm_module {

State::State(const uint64_t& ceiling_msg_size)
    : ceiling_msg_size_(ceiling_msg_size) {}

State::~State() {}

}  // namespace shm_module
