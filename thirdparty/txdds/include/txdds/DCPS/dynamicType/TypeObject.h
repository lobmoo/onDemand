/*
 * @Author       : tangfuqi tangfuqi@baosight.com
 * @Date         : 2025-04-28 13:37:52
 * @FilePath     : /TXDDS/include/DCPS/dynamicType/TypeObject.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#pragma once
#include <bitset>
#include <vector>
#include "TypeIdentifier.h"
#include "CommonType.h"
#include <map>

namespace BaoSky::Cdr
{
    class SerializeCdr;
    class DeserializeCdr;
    class CdrSizeCalculator;
}
using namespace BaoSky::Cdr;
namespace BaoSky::dds
{

    // class MinimalAliasType
    // {
    // public:
    //     AliasTypeFlag m_alias_flags;
    //     MinimalAliasHeader m_header;
    //     MinimalAliasBody m_body;
    // };
    /// cdr
    class TypeFlag
    {
    public:
        uint16_t m_TypeFlag;
        uint32_t ANNOTATION_STR_VALUE_MAX_LEN = 128;
        uint32_t ANNOTATION_OCTETSEC_VALUE_MAX_LEN = 128;
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
    };
    typedef uint32_t LBound;
    typedef std::vector<LBound> LBoundSeq;
    typedef TypeFlag StructTypeFlag;     // All flags apply
    typedef TypeFlag UnionTypeFlag;      // All flags apply
    typedef TypeFlag CollectionTypeFlag; // Unused. No flags apply
    typedef TypeFlag AnnotationTypeFlag; // Unused. No flags apply
    typedef TypeFlag AliasTypeFlag;      // Unused. No flags apply
    typedef TypeFlag EnumTypeFlag;       // Unused. No flags apply
    typedef TypeFlag BitmaskTypeFlag;    // Unused. No flags apply
    typedef TypeFlag BitsetTypeFlag;     // Unused. No flags apply
    class CommonCollectionElement
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CollectionElementFlag m_element_flags;
        TypeIdentifier m_type;
    };
    class CommonArrayHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        LBoundSeq m_bound_seq;
    };

    typedef MemberFlag AliasMemberFlag;
    class CommonAliasBody
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        AliasMemberFlag m_related_flags;
        TypeIdentifier m_related_type;
    };

    class ExtendedAnnotationParameterValue
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
    };
    class AnnotationParameterValue
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        char m__d = 0;
        bool m_boolean_value = false;
        uint8_t m_byte_value = 0;
        int16_t m_int16_value = 0;
        uint16_t m_uint_16_value = 0;
        int32_t m_int32_value = 0;
        uint32_t m_uint32_value = 0;
        int64_t m_int64_value = 0;
        uint64_t m_uint64_value = 0;
        float m_float32_value = 0;
        double m_float64_value = 0;
        long double m_float128_value = 0;
        char m_char_value = 0;
        wchar_t m_wchar_value = 0;
        int32_t m_enumerated_value = 0;
        std::string m_string8_value;
        std::wstring m_string16_value;
        ExtendedAnnotationParameterValue m_extended_value;
    };
    typedef std::array<uint8_t, 4> NameHash;
    class AppliedAnnotationParameter
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        NameHash m_paramname_hash;
        AnnotationParameterValue m_value;
    };
    typedef std::vector<AppliedAnnotationParameter> AppliedAnnotationParameterSeq;
    class AppliedAnnotation
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        TypeIdentifier m_annotation_typeid;
        AppliedAnnotationParameterSeq m_param_seq;
    };

    typedef std::vector<AppliedAnnotation> AppliedAnnotationSeq;
    typedef std::string QualifiedTypeName;
    class AppliedVerbatimAnnotation
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        std::string m_placement;
        std::string m_language;
        std::string m_text;
    };
    class AppliedBuiltinTypeAnnotations
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        AppliedVerbatimAnnotation m_verbatim;
    };

    class CompleteTypeDetail
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        AppliedBuiltinTypeAnnotations m_ann_builtin;
        AppliedAnnotationSeq m_ann_custom;
        QualifiedTypeName m_type_name;
    };
    class CompleteStructHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        TypeIdentifier m_base_type;
        CompleteTypeDetail m_detail;
    };
    typedef uint32_t MemberId;
    typedef MemberFlag StructMemberFlag;
    typedef std::string MemberName;
    class CommonStructMember
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        MemberId m_member_id;
        StructMemberFlag m_member_flags;
        TypeIdentifier m_member_type_id;
    };

    class AppliedBuiltinMemberAnnotations
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        std::string m_unit{};
        AnnotationParameterValue m_min{};
        AnnotationParameterValue m_max{};
        std::string m_hash_id{};
    };
    class CompleteMemberDetail
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        MemberName m_name{};
        AppliedBuiltinMemberAnnotations m_ann_builtin{};
        AppliedAnnotationSeq m_ann_custom{};
    };

    class CompleteStructMember
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonStructMember m_common{};
        CompleteMemberDetail m_detail{};
    };
    typedef std::vector<CompleteStructMember> CompleteStructMemberSeq;
    class CompleteStructType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        StructTypeFlag m_struct_flags;
        CompleteStructHeader m_header;
        CompleteStructMemberSeq m_member_seq;
    };
    class CompleteAliasHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);

        CompleteTypeDetail m_detail;
    };
    class CompleteAliasBody
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonAliasBody m_common;
        AppliedBuiltinMemberAnnotations m_ann_builtin;
        AppliedAnnotationSeq m_ann_custom;
    };
    class CompleteAliasType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        AliasTypeFlag m_alias_flags;
        CompleteAliasHeader m_header;
        CompleteAliasBody m_body;
    };
    class CompleteArrayHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonArrayHeader m_common;
        CompleteTypeDetail m_detail;
    };
    class CompleteElementDetail
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        AppliedBuiltinMemberAnnotations m_ann_builtin;
        AppliedAnnotationSeq m_ann_custom;
    };
    class CompleteCollectionElement
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonCollectionElement m_common;
        CompleteElementDetail m_detail;
    };
    class CompleteArrayType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CollectionTypeFlag m_collection_flag;
        CompleteArrayHeader m_header;
        CompleteCollectionElement m_element;
    };
    class CommonCollectionHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        LBound m_bound;
    };
    class CompleteCollectionHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonCollectionHeader m_common;
        CompleteTypeDetail m_detail;
    };
    class CompleteSequenceType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CollectionTypeFlag m_collection_flag;
        CompleteCollectionHeader m_header;
        CompleteCollectionElement m_element;
    };

    class CompleteMapType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CollectionTypeFlag m_collection_flag;
        CompleteCollectionHeader m_header;
        CompleteCollectionElement m_key;
        CompleteCollectionElement m_element;
    };
    class CompleteAnnotationHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        QualifiedTypeName m_annotation_name;
    };
    class CommonAnnotationParameter
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        AnnotationParameterFlag m_member_flags;
        TypeIdentifier m_member_type_id;
    };
    class CompleteAnnotationParameter
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonAnnotationParameter m_common;
        MemberName m_name;
        AnnotationParameterValue m_default_value;
    };
    typedef std::vector<CompleteAnnotationParameter> CompleteAnnotationParameterSeq;
    class CompleteAnnotationType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        AnnotationTypeFlag m_annotation_flag;
        CompleteAnnotationHeader m_header;
        CompleteAnnotationParameterSeq m_member_seq;
    };
    class CompleteExtendedType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
    };
    class CompleteUnionHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CompleteTypeDetail m_detail;
    };
    class CommonDiscriminatorMember
    {
        public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        UnionDiscriminatorFlag m_member_flags;
        TypeIdentifier m_type_id;
    };
    class CompleteDiscriminatorMember
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonDiscriminatorMember m_common;
        AppliedBuiltinTypeAnnotations m_ann_builtin;
        AppliedAnnotationSeq m_ann_custom;
    };
    typedef std::vector<int32_t> UnionCaseLabelSeq;
    class CommonUnionMember
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        MemberId m_member_id;
        UnionMemberFlag m_member_flags;
        TypeIdentifier m_type_id;
        UnionCaseLabelSeq m_label_seq;
    };
    class CompleteUnionMember
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonUnionMember m_common;
        CompleteMemberDetail m_detail;
    };
    typedef std::vector<CompleteUnionMember> CompleteUnionMemberSeq;
    class CompleteUnionType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        UnionTypeFlag m_union_flags;
        CompleteUnionHeader m_header;
        CompleteDiscriminatorMember m_discriminator;
        CompleteUnionMemberSeq m_member_seq;
    };
    class CompleteBitsetHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        TypeIdentifier m_base_type;
        CompleteTypeDetail m_detail;
    };
    class CommonBitfield
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        uint16_t m_position;
        BitsetMemberFlag m_flags;
        octet m_bitcount;
        TypeKind m_holder_type;
    };
    class CompleteBitfield
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonBitfield m_common;
        CompleteMemberDetail m_detail;
    };
    typedef std::vector<CompleteBitfield> CompleteBitfieldSeq;
    class CompleteBitsetType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        BitsetTypeFlag m_bitset_flags;
        CompleteBitsetHeader m_header;
        CompleteBitfieldSeq m_field_seq;
    };
    typedef uint16_t BitBound;
    class CommonEnumeratedHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        BitBound m_bit_bound;
    };
    class CompleteEnumeratedHeader
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonEnumeratedHeader m_common;
        CompleteTypeDetail m_detail;
    };
    class CommonBitflag
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        uint16_t m_position;
        BitflagFlag m_flags;
    };
    class CompleteBitflag
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonBitflag m_common;
        CompleteMemberDetail m_detail;
    };
    typedef CompleteEnumeratedHeader CompleteBitmaskHeader;
    typedef std::vector<CompleteBitflag> CompleteBitflagSeq;
    class CompleteBitmaskType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        BitmaskTypeFlag m_bitmask_flags;
        CompleteBitmaskHeader m_header;
        CompleteBitflagSeq m_flag_seq;
    };
    class CommonEnumeratedLiteral
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        int32_t m_value;
        EnumeratedLiteralFlag m_flags;
    };
    class CompleteEnumeratedLiteral
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        CommonEnumeratedLiteral m_common;
        CompleteMemberDetail m_detail;
    };
    typedef std::vector<CompleteEnumeratedLiteral> CompleteEnumeratedLiteralSeq;
    class CompleteEnumeratedType
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        EnumTypeFlag m_enum_flags;
        CompleteEnumeratedHeader m_header;
        CompleteEnumeratedLiteralSeq m_literal_seq;
    };
    class CompleteTypeObject
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);
        octet m__d;
        CompleteAliasType m_alias_type;
        CompleteAnnotationType m_annotation_type;
        CompleteStructType m_struct_type;
        CompleteUnionType m_union_type;
        CompleteBitsetType m_bitset_type;
        CompleteSequenceType m_sequence_type;
        CompleteArrayType m_array_type;
        CompleteMapType m_map_type;
        CompleteEnumeratedType m_enumerated_type;
        CompleteBitmaskType m_bitmask_type;
        CompleteExtendedType m_extended_type;
    };
    class TypeObject
    {
    public:
        void Serialize(SerializeCdr &scdr) const;
        size_t GetCdrSerializedSize(CdrSizeCalculator &csc);
        void Deserialize(DeserializeCdr &dscdr);

    public:
        octet m_d;
        CompleteTypeObject mComplete;
    };

}