/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-03-12 10:11:16
 * @FilePath     : /txcdr/include/Enums.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#ifndef TXCDR_ENUM_H
#define TXCDR_ENUM_H
namespace BaoSky::Cdr
{
    typedef enum
    {
        // Common CORBA CDR serialization.
        CORBA_CDR = 0,
        // DDS CDR serialization.
        DDS_CDR = 1, // 实现上没有特殊处理
                     //  XCDRv1 encoding defined by standard DDS X-Types 1.3
        XCDRv1 = 2,
        // XCDRv2 encoding defined by standard DDS X-Types 1.3
        XCDRv2 = 3
    } CdrVersion;

    typedef enum
    {
        BIG_ENDIANNESS = 0x0,
        LITTLE_ENDIANNESS = 0x1
    } Endianness;

    typedef enum 
    {
        PLAIN_CDR = 0x0,
        PL_CDR = 0x2, // 暂时不用(unsupported)
        PLAIN_CDR2 = 0x6,
        DELIMIT_CDR2 = 0x8, // 实际实现和PLAIN_CDR2没有不同
        PL_CDR2 = 0xa       // 暂时不用(unsupported)
    } EncodingAlgorithmFlag;
}
#endif