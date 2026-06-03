/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-15 18:24:38
 * @FilePath: /TXDDS/include/DCPS/core/policy/QosPolicy.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_DCPS_QOSPOLICY_H_
#define _TXDDS_DCPS_QOSPOLICY_H_
#include <txdds/RTPS/common/RTPSMessageTypes.h>
#include <txdds/RTPS/common/Parameter.h>
#include <txdds/RTPS/common/Duration.h>
#include <txdds/RTPS/common/LocatorList_t.h>
#include <txdds/RTPS/common/PortParameters.h>
#include <txdds/RTPS/common/GuidPrefix.h>
#include <txdds/RTPS/attributes/BuiltinAttributes.h>
#include <txdds/RTPS/resources/ResourceManagement.h>
#include <txdds/RTPS/message/CDRMessageOperator.h>
#include "txdds/txddsexport.h"
#include <vector>
#include <string>

namespace BaoSky::dds
{
    using Duration = BaoSky::rtps::Duration;
#define PARAMETER_KIND_LENGTH 4
    enum QosPolicyId_t : uint32_t
    {
        INVALID_QOS_POLICY_ID = 0, //< Does not refer to any valid QosPolicy

        // Standard QosPolicies
        USERDATA_QOS_POLICY_ID,            //< UserDataQosPolicy
        DURABILITY_QOS_POLICY_ID,          //< DurabilityQosPolicy
        PRESENTATION_QOS_POLICY_ID,        //< PresentationQosPolicy
        DEADLINE_QOS_POLICY_ID,            //< DeadlineQosPolicy
        LATENCYBUDGET_QOS_POLICY_ID,       //< LatencyBudgetQosPolicy
        OWNERSHIP_QOS_POLICY_ID,           //< OwnershipQosPolicy
        OWNERSHIPSTRENGTH_QOS_POLICY_ID,   //< OwnershipStrengthQosPolicy
        LIVELINESS_QOS_POLICY_ID,          //< LivelinessQosPolicy
        TIMEBASEDFILTER_QOS_POLICY_ID,     //< TimeBasedFilterQosPolicy
        PARTITION_QOS_POLICY_ID,           //< PartitionQosPolicy
        RELIABILITY_QOS_POLICY_ID,         //< ReliabilityQosPolicy
        DESTINATIONORDER_QOS_POLICY_ID,    //< DestinationOrderQosPolicy
        HISTORY_QOS_POLICY_ID,             //< HistoryQosPolicy
        RESOURCELIMITS_QOS_POLICY_ID,      //< ResourceLimitsQosPolicy
        ENTITYFACTORY_QOS_POLICY_ID,       //< EntityFactoryQosPolicy
        WRITERDATALIFECYCLE_QOS_POLICY_ID, //< WriterDataLifecycleQosPolicy
        READERDATALIFECYCLE_QOS_POLICY_ID, //< ReaderDataLifecycleQosPolicy
        TOPICDATA_QOS_POLICY_ID,           //< TopicDataQosPolicy
        GROUPDATA_QOS_POLICY_ID,           //< GroupDataQosPolicy
        TRANSPORTPRIORITY_QOS_POLICY_ID,   //< TransportPriorityQosPolicy
        LIFESPAN_QOS_POLICY_ID,            //< LifespanQosPolicy
        DURABILITYSERVICE_QOS_POLICY_ID,   //< DurabilityServiceQosPolicy

        // TXDDS Extensions
        WIREPROTOCOLCONFIG_QOS_POLICY_ID, //< WireProtocolConfigQos

        NEXT_QOS_POLICY_ID //< Keep always the last element. For internal use only
    };
    class QosPolicy
    {
    public:
        QosPolicy() : mSendAlways(false)
        {
        }

        QosPolicy(bool isSendAlways) : mSendAlways(isSendAlways)
        {
        }

        /**
         * @brief Destructor
         */
        virtual ~QosPolicy() = default;
        inline bool GetSendAlways()
        {
            return mSendAlways;
        }

    private:
        bool mSendAlways;
        std::string name;
    };

    enum HistoryQosPolicyKind : BaoSky::rtps::octet
    {
        KEEP_LAST_HISTORY_QOS,
        KEEP_ALL_HISTORY_QOS
    };

    class HistoryQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        HistoryQosPolicy()
            : BaoSky::rtps::Parameter(BaoSky::rtps::ParameterId::PID_HISTORY, PARAMETER_KIND_LENGTH + 4),
              QosPolicy(true),
              mKind(KEEP_LAST_HISTORY_QOS),
              mDepth(1)
        {
        }
        HistoryQosPolicyKind mKind;
        int32_t mDepth;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override
        {
            if (!BaoSky::rtps::Parameter::AddToCDRMsg(cdrMsg))
            {
                return false;
            }
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, mKind);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddInt32(cdrMsg, mDepth);
            return true;
        }
    };

    enum ReliabilityQosPolicyKind : BaoSky::rtps::octet
    {
        BEST_EFFORT_RELIABILITY_QOS = 0x01,
        RELIABLE_RELIABILITY_QOS = 0x02
    };

    class ReliabilityQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        ReliabilityQosPolicy()
            : BaoSky::rtps::Parameter(BaoSky::rtps::ParameterId::PID_RELIABILITY, PARAMETER_KIND_LENGTH + PARAMETER_TIME_LENGTH),
              QosPolicy(true),
              mKind(BEST_EFFORT_RELIABILITY_QOS),
              mMaxBlockingTime{0, 100000000} // max_blocking_time = 100ms
        {
        }
        ReliabilityQosPolicyKind mKind;
        Duration mMaxBlockingTime;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override
        {
            if (!BaoSky::rtps::Parameter::AddToCDRMsg(cdrMsg))
            {
                return false;
            }
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, mKind);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddDuration(cdrMsg, mMaxBlockingTime);
            return true;
        }

        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength)
        {
            bool valid = true;
            mLength = paraLength;
            rtps::octet kind;
            valid &= BaoSky::rtps::CDRMessageOperator::ReadOctet(cdrMsg, kind);
            mKind = (ReliabilityQosPolicyKind)kind;
            if (cdrMsg.pos + 3 <= cdrMsg.length)
            {
                cdrMsg.pos += 3;
            }
            else
            {
                return false;
            }
            valid &= BaoSky::rtps::CDRMessageOperator::ReadDuration(cdrMsg, mMaxBlockingTime);
            return valid;
        }
        static std::shared_ptr<BaoSky::rtps::Parameter> CreateInstance()
        {
            return std::static_pointer_cast<Parameter>(std::make_shared<ReliabilityQosPolicy>());
        }
    };
    enum DurabilityQosPolicyKind : rtps::octet
    {
        VOLATILE_DURABILITY_QOS,
        TRANSIENT_LOCAL_DURABILITY_QOS,
        TRANSIENT_DURABILITY_QOS,
        PERSISTENT_DURABILITY_QOS
    };

    class DurabilityQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        DurabilityQosPolicy() : BaoSky::rtps::Parameter(BaoSky::rtps::ParameterId::PID_DURABILITY, PARAMETER_KIND_LENGTH), QosPolicy(true), mKind(VOLATILE_DURABILITY_QOS)
        {
        }
        DurabilityQosPolicyKind mKind;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override
        {
            if (!BaoSky::rtps::Parameter::AddToCDRMsg(cdrMsg))
            {
                return false;
            }
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, mKind);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            BaoSky::rtps::CDRMessageOperator::AddOctet(cdrMsg, 0);
            return true;
        }

        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength)
        {
            bool valid = true;
            mLength = paraLength;
            rtps::octet kind;
            valid &= BaoSky::rtps::CDRMessageOperator::ReadOctet(cdrMsg, kind);
            mKind = (DurabilityQosPolicyKind)kind;
            if (cdrMsg.pos + 3 <= cdrMsg.length)
            {
                cdrMsg.pos += 3;
            }
            else
            {
                return false;
            }
            return valid;
        }

        static std::shared_ptr<BaoSky::rtps::Parameter> CreateInstance()
        {
            return std::static_pointer_cast<Parameter>(std::make_shared<DurabilityQosPolicy>());
        }
    };

    class UserDataQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        std::vector<BaoSky::rtps::octet> mValue;
    };

    class TopicDataQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        std::vector<BaoSky::rtps::octet> mValue;
    };

    class GroupDataQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        std::vector<BaoSky::rtps::octet> mValue;
    };

    class LatencyBudgetQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        Duration mDuration;
    };

    class DeadlineQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        Duration mPeriod;
    };

    class TimeBasedFilterQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        Duration mMinimumSeparation;
    };

    class EntityFactoryQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        bool mAutoEnableCreatedEntities = true;
    };

    class PartitionQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        PartitionQosPolicy() : BaoSky::rtps::Parameter(BaoSky::rtps::ParameterId::PID_PARTITION, 0), QosPolicy(true)
        {
        }
        std::vector<std::string> mName;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override
        {
            mLength += 4;
            for (const auto &it : mName)
            {
                mLength += 4;
                mLength += static_cast<uint16_t>(it.size());
                mLength = (mLength + 3) & ~3;
            }
            if (!BaoSky::rtps::Parameter::AddToCDRMsg(cdrMsg))
            {
                return false;
            }
            BaoSky::rtps::CDRMessageOperator::AddUint32(cdrMsg, mName.size());
            for (const auto &it : mName)
            {
                BaoSky::rtps::CDRMessageOperator::AddString(cdrMsg, it);
            }
            return true;
        }

        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength)
        {
            bool valid = true;
            mLength = paraLength;
            uint32_t size = 0;
            valid &= BaoSky::rtps::CDRMessageOperator::ReadUint32(cdrMsg, size);
            if (!valid)
            {
                return valid;
            }
            for (uint32_t i = 0; i < size; i++)
            {
                std::string str = "";
                valid &= BaoSky::rtps::CDRMessageOperator::ReadString(cdrMsg, str);
                if (valid)
                {
                    mName.push_back(str);
                }
                else
                {
                    return false;
                }
            }
            return valid;
        }
        static std::shared_ptr<BaoSky::rtps::Parameter> CreateInstance()
        {
            return std::static_pointer_cast<Parameter>(std::make_shared<PartitionQosPolicy>());
        }
    };

    enum LivelinessQosPolicyKind
    {
        AUTOMATIC_LIVELINESS_QOS,
        MANUAL_BY_PARTICIPANT_LIVELINESS_QOS,
        MANUAL_BY_TOPIC_LIVELINESS_QOS
    };

    class LivelinessQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        Duration mLeaseDuration;
        LivelinessQosPolicyKind mKind = AUTOMATIC_LIVELINESS_QOS;
    };

    class ResourceLimitsQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        long mMaxSamples = 1000;
        long mMaxInstance = 10;
        long mMaxSamplesPerInstance = 100;
        long mAllocatedSamples = 1;
        long mExtraSamples = 1;
        uint16_t mMaxMessageLength = 65000;
    };

    class DurabilityServiceQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        Duration mServiceCleanupDelay;
        HistoryQosPolicyKind mHistoryKind;
        long mHistoryDepth;
        long mMaxSamples;
        long mMaxInstance;
        long mMaxSamplesPerInstance;
    };

    enum DestinationOrderQosPolicyKind
    {
        BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS,
        BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS
    };

    class DestinationOrderQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        DestinationOrderQosPolicyKind mKind = BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS;
    };

    class LifespanQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        Duration mDuration;
    };

    enum OwnershipQosPolicyKind
    {
        SHARED_OWNERSHIP_QOS,
        EXCLUSIVE_OWNERSHIP_QOS
    };

    class OwnershipQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        OwnershipQosPolicyKind mKind = SHARED_OWNERSHIP_QOS;
    };

    class OwnershipStrengthQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        long mValue;
    };

    class WriterDataLifecycleQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        bool mAutoDisposeUnregisteredInstances;
    };

    class ReaderDataLifecycleQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        Duration mAutopurgeNowriterSamplesDelay;
        Duration mAutopurgeDisposedSamplesDelay;
    };

    enum PresentationQosPolicyAccessScopeKind
    {
        INSTANCE_PRESENTATION_QOS,
        TOPIC_PRESENTATION_QOS,
        GROUP_PRESENTATION_QOS
    };

    class PresentationQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        PresentationQosPolicyAccessScopeKind mAccessScope = INSTANCE_PRESENTATION_QOS;
        bool mCoherentAccess = false;
        bool mOrderedAccess = false;
    };

    class TransportPriorityQosPolicy : public BaoSky::rtps::Parameter, public QosPolicy
    {
    public:
        long mValue;
    };

    //! Qos Policy that configures the wire protocol
    class WireProtocolConfigQos : public QosPolicy
    {

    public:
        /**
         * @brief Constructor
         */
        WireProtocolConfigQos() : QosPolicy(false), participantId(-1)
        {
        }

        /**
         * @brief Destructor
         */
        virtual ~WireProtocolConfigQos() = default;

        //! Optionally allows user to define the GuidPrefix
        BaoSky::rtps::GuidPrefix prefix;

        //! Participant ID <br> By default, -1.
        int32_t participantId;

        //! Builtin parameters.
        BaoSky::rtps::BuiltinAttributes builtin;

        //! Port Parameters
        BaoSky::rtps::PortParameters port;

        /**
         * Default list of Unicast Locators to be used for any Endpoint defined inside this RTPSParticipant in the case
         * that it was defined with NO UnicastLocators. At least ONE locator should be included in this list.
         */
        BaoSky::rtps::LocatorList defaultUnicastLocatorList;

        /**
         * Default list of Multicast Locators to be used for any Endpoint defined inside this RTPSParticipant in the
         * case that it was defined with NO MulticastLocators. This is usually left empty.
         */
        BaoSky::rtps::LocatorList defaultMulticastLocatorList;
    };

    class RTPSEndPointQos
    {
    public:
        //! Unicast locator list
        rtps::LocatorList unicastLocatorList;
        //! Multicast locator list
        rtps::LocatorList multicastLocatorList;
        //! Remote locator list
        rtps::LocatorList mRemoteLocatorList;

        int16_t mEntity = -1;

        BaoSky::rtps::MemoryManagementPolicy mHistoryMemoryPolicy = BaoSky::rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;

        int16_t mMaxMatchedLimit = -1;

        bool mIsSHMDirect = false;
    };
}
#endif