/**
 * @file dds_abstraction.h
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

#ifndef DDS_ABSTRACTION_H
#define DDS_ABSTRACTION_H

#ifdef USE_FASTDDS
#include "fastdds_wrapper/fastdds_node.h"
#include "fastdds_wrapper/fastdds_qos_config.h"
#include "fastdds_wrapper/fastdds_listeners.h"
#include <fastdds/dds/subscriber/SampleInfo.hpp>

namespace DdsWrapper = FastddsWrapper;
using SampleInfo = eprosima::fastdds::dds::SampleInfo;

namespace FastddsWrapper {
    using DataNode = FastDataNode;
    template <typename T>
    using DDSTopicWriter = FastDDSTopicWriter<T>;
    template <typename T>
    using DDSTopicReader = FastDDSTopicReader<T>;
}

#elif defined(USE_TXDDS)
#include "txdds_wrapper/txdds_node.h"
#include "txdds_wrapper/txdds_qos_config.h"
#include "txdds_wrapper/txdds_listeners.h"
#include <txdds/DCPS/subscriber/SampleInfo.h>

namespace DdsWrapper = TxddsWrapper;
using SampleInfo = BaoSky::dds::SampleInfo;

namespace TxddsWrapper {
    using DataNode = TXDDSNode;
    template <typename T>
    using DDSTopicWriter = TXDDSTopicWriter<T>;
    template <typename T>
    using DDSTopicReader = TXDDSTopicReader<T>;
}

#else
#error "Please define either USE_FASTDDS or USE_TXDDS to select DDS implementation"
#endif

#endif // DDS_ABSTRACTION_H