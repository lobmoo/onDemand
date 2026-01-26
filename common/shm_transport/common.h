/**
 * @file common.h
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
#ifndef SHM_MODULE_COMMON_H_
#define SHM_MODULE_COMMON_H_

#include <iostream>
#include <cstdint>
#include <cstring>

// 简化的日志宏，替代 Apollo 的 AERROR, AINFO, ADEBUG
#define SHM_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl
#define SHM_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define SHM_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl

// 替代 RETURN_VAL_IF_NULL 宏
#define RETURN_VAL_IF_NULL(ptr, val) \
  if (ptr == nullptr) { \
    SHM_ERROR("Null pointer check failed"); \
    return val; \
  }

#endif  // SHM_MODULE_COMMON_H_
