/*
 * @Author       : tangfuqi tangfuqi@baosight.com
 * @Date         : 2025-04-28 13:37:40
 * @FilePath     : /TXDDS/include/DCPS/dynamicType/TypeIdentifier.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#pragma once
#include <stdint.h>
#include <vector>
#include <bitset>
#include "txcdr/Cdr.h"
#include <string.h>
namespace BaoSky::Cdr
{
    class SerializeCdr;
    class DeserializeCdr;
    class CdrSizeCalculator;
}
using namespace BaoSky::Cdr;
namespace BaoSky::dds
{
    typedef uint32_t LBound;
    typedef std::vector<LBound> LBoundSeq;
    typedef unsigned char SBound;
    using octet = unsigned char;
    typedef std::vector<SBound> SBoundSeq;
    typedef octet EquivalenceKind;
    class MemberFlag
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        std::bitset<16> m_MemberFlag;
    };

    typedef MemberFlag CollectionElementFlag;  // T1, T2, X
    typedef MemberFlag StructMemberFlag;       // T1, T2, O, M, K, X
    typedef MemberFlag UnionMemberFlag;        // T1, T2, D, X
    typedef MemberFlag UnionDiscriminatorFlag; // T1, T2, K
    typedef MemberFlag EnumeratedLiteralFlag;
    typedef MemberFlag AnnotationParameterFlag;
    typedef MemberFlag AliasMemberFlag;
    typedef MemberFlag BitflagFlag;
    typedef MemberFlag BitsetMemberFlag;

    class PlainCollectionHeader
    {
    public:
        EquivalenceKind m_equiv_kind;
        CollectionElementFlag m_element_flags;
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
    };
    class TypeIdentifier;

    class StringSTypeDefn
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        SBound m_bound;
    };
    class StringLTypeDefn
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        LBound m_bound;
    };

    class PlainSequenceSElemDefn
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        PlainCollectionHeader m_header;
        SBound m_bound;
        TypeIdentifier *m_element_identifier{};
        ~PlainSequenceSElemDefn()
        {
        }
    };

    class PlainSequenceLElemDefn
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        PlainCollectionHeader m_header;
        LBound m_bound;
        TypeIdentifier *m_element_identifier{};
        bool mDeserializeCreated = false;
        ~PlainSequenceLElemDefn()
        {
        }
    };

    class PlainArraySElemDefn
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        PlainCollectionHeader m_header;
        SBoundSeq m_array_bound_seq;
        TypeIdentifier *m_element_identifier{};
        bool mDeserializeCreated = false;
        ~PlainArraySElemDefn()
        {
        }
    };

    class PlainArrayLElemDefn
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        PlainCollectionHeader m_header;
        LBoundSeq m_array_bound_seq;
        TypeIdentifier *m_element_identifier{};
        bool mDeserializeCreated = false;
        ~PlainArrayLElemDefn()
        {
        }
    };

    class PlainMapSTypeDefn
    {
    public:
        PlainMapSTypeDefn()=default;
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        PlainCollectionHeader m_header;
        SBound m_bound;
        TypeIdentifier *m_element_identifier{};
        CollectionElementFlag m_key_flags;
        TypeIdentifier *m_key_identifier{};
        bool mDeserializeElementIdentifier = false;
        bool mDeserializeKeyIdentifier = false;
        ~PlainMapSTypeDefn()
        {
        }
    };
    typedef octet EquivalenceHash[14];
    class TypeObjectHashId
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        uint8_t m__d;
        EquivalenceHash m_hash;
    };
    class PlainMapLTypeDefn
    {
    public:
        PlainMapLTypeDefn()
        {
        }
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        PlainCollectionHeader m_header;
        LBound m_bound;
        TypeIdentifier *m_element_identifier{};
        CollectionElementFlag m_key_flags;
        TypeIdentifier *m_key_identifier{};
        bool mDeserializeCreated = false;
        ~PlainMapLTypeDefn()
        {
        }
    };
    class StronglyConnectedComponentId
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        TypeObjectHashId m_sc_component_id;
        int32_t m_scc_length;
        int32_t m_scc_index;
    };
    class ExtendedTypeDefn
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
    };
    /// @brief serialize
    class TypeIdentifier
    {
    public:
        TypeIdentifier() = default;
        // void _d(octet __d)
        // {
        //     bool b = false;
        //     m__d = __d;
        // }

        // static size_t getCdrSerializedSize(
        //     const TypeIdentifier &data,
        //     size_t current_alignment);
        // void serialize(
        //     Cdr &scdr) const;
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        bool operator==(
            const TypeIdentifier &other) const;

    public:
        StringSTypeDefn m_string_sdefn;
        StringLTypeDefn m_string_ldefn;
        PlainSequenceSElemDefn m_seq_sdefn;
        PlainSequenceLElemDefn m_seq_ldefn;
        PlainArraySElemDefn m_array_sdefn;
        PlainArrayLElemDefn m_array_ldefn;
        PlainMapSTypeDefn m_map_sdefn;
        PlainMapLTypeDefn m_map_ldefn;
        StronglyConnectedComponentId m_sc_component_id;
        ExtendedTypeDefn m_extended_defn;
        octet m__d{};
        // identifier hash
        octet m_equivalence_hash[14];
    };

}