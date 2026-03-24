/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-03-28 16:50:41
 * @FilePath: /TXDDS/include/DCPS/dynamicType/CommonType.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */

#pragma once
#include <cstdint>
#include <string>
namespace BaoSky::dds
{
    typedef uint32_t MemberId;
    const MemberId MEMBER_ID_INVALID = 0x0fffffff;
    using octet = unsigned char;
    typedef octet TypeKind;
    const octet TypeKind_NONE = 0x00;
    const octet TypeKind_BOOLEAN = 0x01;
    const octet TypeKind_BYTE = 0x02;
    const octet TypeKind_INT16 = 0x03;
    const octet TypeKind_INT32 = 0x04;
    const octet TypeKind_INT64 = 0x05;
    const octet TypeKind_UINT16 = 0x06;
    const octet TypeKind_UINT32 = 0x07;
    const octet TypeKind_UINT64 = 0x08;
    const octet TypeKind_FLOAT32 = 0x09;
    const octet TypeKind_FLOAT64 = 0x0A;
    const octet TypeKind_FLOAT128 = 0x0B;
    const octet TypeKind_INT8 = 0x0C;
    const octet TypeKind_UINT8 = 0x0D;
    const octet TypeKind_CHAR8 = 0x10;
    const octet TypeKind_CHAR16 = 0x11;
    const octet TypeKind_STRING8 = 0x20;
    const octet TypeKind_STRING16 = 0x21;
    const octet TypeKind_ALIAS = 0x30;
    const octet TypeKind_ENUM = 0x40;
    const octet TypeKind_BITMASK = 0x41;
    const octet TypeKind_ANNOTATION = 0x50;
    const octet TypeKind_STRUCTURE = 0x51;
    const octet TypeKind_UNION = 0x52;
    const octet TypeKind_BITSET = 0x53;
    const octet TypeKind_SEQUENCE = 0x60;
    const octet TypeKind_ARRAY = 0x61;
    const octet TypeKind_MAP = 0x62;
    const octet EquivalenceKind_MINIMAL = 0xF1;  // 0x1111 0001
    const octet EquivalenceKind_COMPLETE = 0xF2; // 0x1111 0010
    const octet EquivalenceKind_BOTH = 0xF3;     // 0x1111 0011
    const octet TypeIdentiferKind_STRING8_SMALL = 0x70;
    const octet TypeIdentiferKind_STRING8_LARGE = 0x71;
    const octet TypeIdentiferKind_STRING16_SMALL = 0x72;
    const octet TypeIdentiferKind_STRING16_LARGE = 0x73;
    const octet TypeIdentiferKind_PLAIN_SEQUENCE_SMALL = 0x80;
    const octet TypeIdentiferKind_PLAIN_SEQUENCE_LARGE = 0x81;
    const octet TypeIdentiferKind_PLAIN_ARRAY_SMALL = 0x90;
    const octet TypeIdentiferKind_PLAIN_ARRAY_LARGE = 0x91;
    const octet TypeIdentiferKind_PLAIN_MAP_SMALL = 0xA0;
    const octet TypeIdentiferKind_PLAIN_MAP_LARGE = 0xA1;
    const octet TypeIdentiferKind_STRONGLY_CONNECTED_COMPONENT = 0xB0;
    const std::string TypeKindName_BOOLEAN = "bool";
    const std::string TypeKindName_INT16 = "int16_t";
    const std::string TypeKindName_UINT16 = "uint16_t";
    const std::string TypeKindName_INT32 = "int32_t";
    const std::string TypeKindName_UINT32 = "uint32_t";
    const std::string TypeKindName_INT64 = "int64_t";
    const std::string TypeKindName_UINT64 = "uint64_t";
    const std::string TypeKindName_CHAR8 = "char";
    const std::string TypeKindName_BYTE = "octet";
    const std::string TypeKindName_INT8 = "int8_t";
    const std::string TypeKindName_UINT8 = "uint8_t";
    const std::string TypeKindName_CHAR16 = "wchar";
    const std::string TypeKindName_CHAR16T = "wchar_t";
    const std::string TypeKindName_FLOAT32 = "float";
    const std::string TypeKindName_FLOAT64 = "double";
    const std::string TypeKindName_FLOAT128 = "longdouble";
    const std::string TypeKindName_STRING8 = "string";
    const std::string TypeKindName_STRING16 = "wstring";
    const std::string TINAME_STRING8_SMALL = "TINAME_STRING8_SMALL";
    const std::string TINAME_STRING8_LARGE = "TINAME_STRING8_LARGE";
    const std::string TINAME_STRING16_SMALL = "TINAME_STRING16_SMALL";
    const std::string TINAME_STRING16_LARGE = "TINAME_STRING16_LARGE";
}
