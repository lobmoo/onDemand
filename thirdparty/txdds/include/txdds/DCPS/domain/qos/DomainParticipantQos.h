/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-02-06 15:01:09
 * @LastEditors: zhouyuan 563733@baosight.com
 * @LastEditTime: 2025-07-28 11:18:35
 * @FilePath     : /TXDDS/include/txdds/DCPS/domain/qos/DomainParticipantQos.h
 * @Description  : 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#ifndef _TXDDS_DCPS_DOMAINPARTICIPANTQOS_H_
#define _TXDDS_DCPS_DOMAINPARTICIPANTQOS_H_

#include "txdds/DCPS/core/policy/QosPolicy.h"
#include "txdds/RTPS/transport/ThreadConfig.h"

namespace BaoSky::dds
{
    class TXDDS_API DomainParticipantQos
    {
    public:
        DomainParticipantQos() = default;
        ~DomainParticipantQos() = default;
        WireProtocolConfigQos wireProtocol;
        BaoSky::rtps::ThreadConfig mEventThreadConfig;
        std::string mName = "";
    };
}
#endif