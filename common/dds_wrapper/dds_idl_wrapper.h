/**
 * @file dds_idl_wrapper.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-12-02
 * 
 * @copyright Copyright (c) 2025  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-12-02     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */

#ifndef DDS_IDL_WRAPPER_H
#define DDS_IDL_WRAPPER_H

#ifdef USE_FASTDDS
// FastDDS
#include "dsf_define_datatypes.hpp"
#include "dsf_define_node.hpp"
#include "dsf_define_nodePubSubTypes.hpp"
#include "dsf_define_messages.hpp"
#include "dsf_define_messagesPubSubTypes.hpp"
#include "dsf_define_var.hpp"
#include "dsf_define_varPubSubTypes.hpp"
#include "dsf_define_model.hpp"
#include "dsf_define_modelPubSubTypes.hpp"
#elif defined(USE_TXDDS)
// TXDDS
#include "dsf_define_datatypes.h"
#include "dsf_define_node.h"
#include "dsf_define_nodePubSubTypes.h"
#include "dsf_define_messages.h"
#include "dsf_define_messagesPubSubTypes.h"
#include "dsf_define_var.h"
#include "dsf_define_varPubSubTypes.h"
#include "dsf_define_model.h"
#include "dsf_define_modelPubSubTypes.h"
#else
#error "Please define either USE_FASTDDS or USE_TXDDS"
#endif

#endif // DDS_IDL_WRAPPER_H