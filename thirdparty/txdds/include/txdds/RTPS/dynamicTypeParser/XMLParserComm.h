/*
 * @Author       : baihaoran 948942547@qq.com
 * @Date         : 2025-05-16 16:25:33
 * @FilePath     : /TXDDS/include/RTPS/DynamicTypeBuilder/XMLParserComm.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#pragma once

namespace BaoSky::rtps::DynamicBuilder
{
    enum class XMLP_ret
    {
        XML_ERROR,
        XML_OK,
        XML_NOK
    };

    extern const char *DEFAULT_XML_PROFILES_FILE;

    extern const char *TX_ROOT;
    extern const char *TX_PROFILES;
    extern const char *TX_TYPES;
    extern const char *TX_TYPE;
    extern const char *TX_LOG;
    extern const char *TX_LIBRARY_SETTINGS;
    extern const char *TX_NAME;

    // TYPES parser
    extern const char *TX_BOOLEAN;
    extern const char *TX_CHAR;
    extern const char *TX_WCHAR;
    extern const char *TX_TBYTE;
    extern const char *TX_OCTET;
    extern const char *TX_UINT8;
    extern const char *TX_INT8;
    extern const char *TX_SHORT;
    extern const char *TX_LONG;
    extern const char *TX_USHORT;
    extern const char *TX_ULONG;
    extern const char *TX_LONGLONG;
    extern const char *TX_ULONGLONG;
    extern const char *TX_FLOAT;
    extern const char *TX_DOUBLE;
    extern const char *TX_LONGDOUBLE;
    extern const char *TX_STRING;
    extern const char *TX_WSTRING;
    extern const char *TX_LITERAL;
    extern const char *TX_STRUCT;
    extern const char *TX_UNION;
    extern const char *TX_SEQUENCE;
    extern const char *TX_MAP;
    extern const char *TX_TYPEDEF;
    extern const char *TX_BITSET;
    extern const char *TX_BITMASK;
    extern const char *TX_ENUM;
    extern const char *TX_CASE;
    extern const char *TX_DEFAULT;
    extern const char *TX_DISCRIMINATOR;
    extern const char *TX_CASE_DISCRIMINATOR;
    extern const char *TX_ARRAY_DIMENSIONS;
    extern const char *TX_STR_MAXLENGTH;
    extern const char *TX_SEQ_MAXLENGTH;
    extern const char *TX_MAP_MAXLENGTH;
    extern const char *TX_MAP_KEY_TYPE;
    extern const char *TX_ENUMERATOR;
    extern const char *TX_NON_BASIC_TYPE;
    extern const char *TX_NON_BASIC_TYPE_NAME;
    extern const char *TX_KEY;
    extern const char *TX_MEMBER;
    extern const char *TX_BITFIELD;
    extern const char *TX_BIT_VALUE;
    extern const char *TX_POSITION;
    extern const char *TX_BIT_BOUND;
    extern const char *TX_BASE_TYPE;

}
