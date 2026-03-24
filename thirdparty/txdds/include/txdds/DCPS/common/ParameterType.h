#ifndef TXDDS_RTPS_PARAMETERTYPE_H
#define TXDDS_RTPS_PARAMETERTYPE_H

#include <txdds/RTPS/common/Parameter.h>
#include <txdds/RTPS/utils/FixedString.h>
#include <txdds/DCPS/common/DomainId.h>
#include <txdds/RTPS/common/RTPSEntityTypes.h>
#include <txdds/RTPS/common/Locator.h>
#include <txdds/RTPS/common/Guid.h>
#include <txdds/RTPS/common/Duration.h>
#include <txdds/RTPS/common/InstanceHandle.h>

namespace BaoSky::dds
{
    using namespace BaoSky::rtps;
    class ProtocolVersionParameter : public BaoSky::rtps::Parameter
    {
    public:
        ProtocolVersionParameter(rtps::ProtocolVersion_t ProtocolVersion = rtps::PROTOCOLVERSION) : BaoSky::rtps::Parameter(rtps::PID_PROTOCOL_VERSION, 4), mProtocolVersion(ProtocolVersion)
        {
        }
        rtps::ProtocolVersion_t mProtocolVersion;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;

        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class VendorIdParameter : public BaoSky::rtps::Parameter
    {
    public:
        VendorIdParameter(rtps::VendorId_t vendorId = rtps::VENDORID_TXDDS) : BaoSky::rtps::Parameter(rtps::PID_VENDOR_ID, 4), mVendorId(vendorId)
        {
        }
        rtps::VendorId_t mVendorId;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class LocatorParameter : public BaoSky::rtps::Parameter
    {
    public:
        LocatorParameter(BaoSky::rtps::ParameterId pid, rtps::Locator locator) : BaoSky::rtps::Parameter(pid, 24), mLocator(locator)
        {
        }

        LocatorParameter() : BaoSky::rtps::Parameter(rtps::PID_SENTINEL, 24)
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        rtps::Locator mLocator;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
    };

    class DefaultUnicastLocatorParameter : public LocatorParameter
    {
    public:
        DefaultUnicastLocatorParameter(rtps::Locator locator) : LocatorParameter(rtps::PID_DEFAULT_UNICAST_LOCATOR, locator)
        {
        }
        DefaultUnicastLocatorParameter() : LocatorParameter(rtps::PID_DEFAULT_UNICAST_LOCATOR, BaoSky::rtps::Locator())
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class DefaultMulticastLocatorParameter : public LocatorParameter
    {
    public:
        DefaultMulticastLocatorParameter(rtps::Locator locator) : LocatorParameter(rtps::PID_DEFAULT_MULTICAST_LOCATOR, locator)
        {
        }
        DefaultMulticastLocatorParameter() : LocatorParameter(rtps::PID_DEFAULT_MULTICAST_LOCATOR, BaoSky::rtps::Locator())
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class MetatrafficMulticastLocatorParameter : public LocatorParameter
    {
    public:
        MetatrafficMulticastLocatorParameter(rtps::Locator locator) : LocatorParameter(rtps::PID_METATRAFFIC_MULTICAST_LOCATOR, locator)
        {
        }
        MetatrafficMulticastLocatorParameter() : LocatorParameter(rtps::PID_METATRAFFIC_MULTICAST_LOCATOR, BaoSky::rtps::Locator())
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class MetatrafficUnicastLocatorParameter : public LocatorParameter
    {
    public:
        MetatrafficUnicastLocatorParameter(rtps::Locator locator) : LocatorParameter(rtps::PID_METATRAFFIC_UNICAST_LOCATOR, locator)
        {
        }
        MetatrafficUnicastLocatorParameter() : LocatorParameter(rtps::PID_METATRAFFIC_UNICAST_LOCATOR, BaoSky::rtps::Locator())
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class ParticipantGuidParameter : public BaoSky::rtps::Parameter
    {
    public:
        ParticipantGuidParameter(rtps::Guid guid) : BaoSky::rtps::Parameter(rtps::PID_PARTICIPANT_GUID, 16), mGuid(guid)
        {
        }
        ParticipantGuidParameter() : BaoSky::rtps::Parameter(rtps::PID_PARTICIPANT_GUID, 16)
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        rtps::Guid mGuid;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;

        static std::shared_ptr<Parameter> CreateInstance();
    };

    class ExpectsInlineQosParameter : public BaoSky::rtps::Parameter
    {
    public:
        ExpectsInlineQosParameter(bool value) : BaoSky::rtps::Parameter(rtps::PID_EXPECTS_INLINE_QOS, 4), mValue(value)
        {
        }
        ExpectsInlineQosParameter() : BaoSky::rtps::Parameter(rtps::PID_EXPECTS_INLINE_QOS, 4)
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool mValue;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class ParticipantLeaseDurationParameter : public BaoSky::rtps::Parameter
    {
    public:
        ParticipantLeaseDurationParameter(rtps::Duration duration) : BaoSky::rtps::Parameter(rtps::PID_PARTICIPANT_LEASE_DURATION, 8), mDuration(duration)
        {
        }
        ParticipantLeaseDurationParameter() : BaoSky::rtps::Parameter(rtps::PID_PARTICIPANT_LEASE_DURATION, 8)
        {
        }
        rtps::Duration mDuration;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class EntityNameParameter : public BaoSky::rtps::Parameter
    {
    public:
        EntityNameParameter(std::string name) : BaoSky::rtps::Parameter(rtps::PID_ENTITY_NAME, ((name.length() + 3) & ~3) + 4), mName(name)
        {
        }
        EntityNameParameter() : BaoSky::rtps::Parameter(rtps::PID_ENTITY_NAME, 0)
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        rtps::EntityName_t mName;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class BuiltInEndPointSetParameter : public BaoSky::rtps::Parameter
    {
    public:
        BuiltInEndPointSetParameter(rtps::BuiltinEndpointSet_t value) : BaoSky::rtps::Parameter(rtps::PID_BUILTIN_ENDPOINT_SET, 4), mValue(value)
        {
        }
        BuiltInEndPointSetParameter() : BaoSky::rtps::Parameter(rtps::PID_BUILTIN_ENDPOINT_SET, 4)
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        rtps::BuiltinEndpointSet_t mValue;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class EndpointGuidParameter : public ParticipantGuidParameter
    {
    public:
        EndpointGuidParameter(rtps::Guid guid) : ParticipantGuidParameter(guid)
        {
            mPid = rtps::PID_ENDPOINT_GUID;
        }
        EndpointGuidParameter() : ParticipantGuidParameter(BaoSky::rtps::Guid())
        {
            mPid = rtps::PID_ENDPOINT_GUID;
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class UnicastLocatorParameter : public LocatorParameter
    {
    public:
        UnicastLocatorParameter(rtps::Locator locator) : LocatorParameter(rtps::PID_UNICAST_LOCATOR, locator)
        {
        }
        UnicastLocatorParameter() : LocatorParameter(rtps::PID_UNICAST_LOCATOR, BaoSky::rtps::Locator())
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class MulticastLocatorParameter : public LocatorParameter
    {
    public:
        MulticastLocatorParameter(rtps::Locator locator) : LocatorParameter(rtps::PID_MULTICAST_LOCATOR, locator)
        {
        }
        MulticastLocatorParameter() : LocatorParameter(rtps::PID_MULTICAST_LOCATOR, BaoSky::rtps::Locator())
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class TopicNameParameter : public BaoSky::rtps::Parameter
    {
    public:
        TopicNameParameter(std::string name) : BaoSky::rtps::Parameter(rtps::PID_TOPIC_NAME, ((name.length() + 3) & ~3) + 4), mName(name)
        {
        }

        TopicNameParameter() : BaoSky::rtps::Parameter(rtps::PID_TOPIC_NAME, 0)
        {
        }
        std::string mName;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class TypeNameParameter : public BaoSky::rtps::Parameter
    {
    public:
        TypeNameParameter(std::string name) : BaoSky::rtps::Parameter(rtps::PID_TYPE_NAME, ((name.length() + 3) & ~3) + 4), mName(name)
        {
        }

        TypeNameParameter() : BaoSky::rtps::Parameter(rtps::PID_TYPE_NAME, 0)
        {
        }
        std::string mName;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class KeyHashParameter : public BaoSky::rtps::Parameter
    {
    public:
        KeyHashParameter(rtps::KeyHash_t keyHash) : BaoSky::rtps::Parameter(rtps::PID_KEY_HASH, 16), mKeyHash(keyHash)
        {
        }
        rtps::KeyHash_t mKeyHash;
        KeyHashParameter() : BaoSky::rtps::Parameter(rtps::PID_KEY_HASH, 16)
        {
        }
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class TypeMaxSizeSerializedParameter : public BaoSky::rtps::Parameter
    {
    public:
        TypeMaxSizeSerializedParameter(uint32_t size) : BaoSky::rtps::Parameter(rtps::PID_TYPE_MAX_SIZE_SERIALIZED, 4), mSize(size)
        {
        }
        TypeMaxSizeSerializedParameter() : BaoSky::rtps::Parameter(rtps::PID_TYPE_MAX_SIZE_SERIALIZED, 4)
        {
        }
        uint32_t mSize;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class StatusInfoParameter : public BaoSky::rtps::Parameter
    {
    public:
        StatusInfoParameter(rtps::octet status) : BaoSky::rtps::Parameter(rtps::PID_STATUS_INFO, 4), mStatus(status)
        {
        }
        StatusInfoParameter() : BaoSky::rtps::Parameter(rtps::PID_STATUS_INFO, 4)
        {
        }
        virtual ~StatusInfoParameter() override = default;
        rtps::octet mStatus;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };

    class DomainIdParameter : public BaoSky::rtps::Parameter
    {
    public:
        DomainIdParameter(uint32_t id) : BaoSky::rtps::Parameter(rtps::PID_DOMAIN_ID, 4), mDomainId(id)
        {
        }
        DomainIdParameter() : BaoSky::rtps::Parameter(rtps::PID_DOMAIN_ID, 4)
        {
        }
        uint32_t mDomainId;
        bool ReadFromCDRMsg(rtps::CDRMessage &cdrMsg, uint16_t paraLength) override;
        bool AddToCDRMsg(rtps::CDRMessage &cdrMsg) override;
        static std::shared_ptr<Parameter> CreateInstance();
    };
}
#endif