/*
 * @Author      : wengjiqing wengjiqing@baosight.com
 * @Date        : 2024-07-25 10:19:09
 * @FilePath: /TXDDS/include/RTPS/message/CDRMessageOperator.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef _TXDDS_RTPS_CDRMESSAGEOPERATOR_H_
#define _TXDDS_RTPS_CDRMESSAGEOPERATOR_H_
#include <txdds/RTPS/common/CDRMessage.h>
#include <txdds/RTPS/message/SubMessageElements.h>
#include <txdds/RTPS/common/Duration.h>

namespace BaoSky::rtps
{
    class TXDDS_API CDRMessageOperator
    {
    public:
        CDRMessageOperator() = delete;
        ~CDRMessageOperator() = delete;
        /**
         * @Description : 将 second 的 buffer 追加到 first 中
         * @param        first: output param
         * @param        second: input param
         * @return       {bool} 追加是否成功
         */
        static bool AppendMsg(CDRMessage &first, CDRMessage &second);

        /**
         * @Description : 将基础类型追加到 cdrMsg 中，其中涉及大小端转换
         * @return       {bool} 追加是否成功
         */
        template <typename T>
        static bool AddPrimitive(CDRMessage &cdrMsg, T element);

        /**
         * @Description : 将从 data 开始，长度为 length 的字节加入到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        data: input param
         * @param        length: input param
         * @param        reverse: input param 是否需要倒叙写入
         * @return       {void}
         */
        static void CopyToBuffer(CDRMessage &cdrMsg, const octet *data, const uint32_t length, bool reverse = false);

        /**
         * @Description : 将一个字节加入到 cdrMsg
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddOctet(CDRMessage &cdrMsg, octet element);

        /**
         * @Description : 追加 R T P S 四个字节到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddProtocolId(CDRMessage &cdrMsg, ProtocolId element);

        /**
         * @Description : 追加协议版本号到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddProtocolVersion(CDRMessage &cdrMsg, ProtocolVersion_t element);

        /**
         * @Description : 追加 VendorId_t 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddVendorId(CDRMessage &cdrMsg, VendorId_t element);

        /**
         * @Description : 追加 guid 前缀到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddGuidPrefix(CDRMessage &cdrMsg, GuidPrefix element);

        /**
         * @Description : 追加 EntityId 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddEntityId(CDRMessage &cdrMsg, EntityId element);

        /**
         * @Description : 追加 SequenceNumber 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddSequenceNumber(CDRMessage &cdrMsg, SequenceNumber element);

        /**
         * @Description : 追加 FragmentNumber_t 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddFragmentNumber(CDRMessage &cdrMsg, FragmentNumber_t element);

        /**
         * @Description : 追加 uint16_t 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddUint16(CDRMessage &cdrMsg, uint16_t element);

        /**
         * @Description : 追加 uint32_t 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddUint32(CDRMessage &cdrMsg, uint32_t element);

        /**
         * @Description : 追加 int32_t 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddInt32(CDRMessage &cdrMsg, int32_t element);

        /**
         * @Description : 追加 ParameterList 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddParameterList(CDRMessage &cdrMsg, ParameterList element);

        /**
         * @Description : 将从 element 开始，长度为 size 的字节加入到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddData(CDRMessage &cdrMsg, octet *element, uint32_t size);

        /**
         * @Description : 追加 SerializedPayload 到 cdrMsg 中,其中包括了 payload 类型、payload data 等信息
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddSerializedPayload(CDRMessage &cdrMsg, SerializedPayload &element);

        /**
         * @Description : 追加 Guid 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddGUID(CDRMessage &cdrMsg, const BaoSky::rtps::Guid &element);

        /**
         * @Description : 追加 Locator 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddLocator(CDRMessage &cdrMsg, Locator element);

        /**
         * @Description : 追加 Duration 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddDuration(CDRMessage &cdrMsg, Duration element);

        /**
         * @Description : 追加 ParameterList 终止符
         * @param        cdrMsg: output param
         * @return       {bool} 追加是否成功
         */
        static bool AddSentinel(CDRMessage &cdrMsg);

        /**
         * @Description : 追加 string 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddString(CDRMessage &cdrMsg, std::string element);

        /**
         * @Description : 从 cdrMsg 中读取一个字节
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadOctet(CDRMessage &cdrMsg, octet &value);

        /**
         * @Description : 从 cdrMsg 中读取长度为 length 的 buffer
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @param        length: input param
         * @return       {bool} 读取是否成功
         */
        static bool ReadData(CDRMessage &cdrMsg, octet *value, uint32_t length);

        /**
         * @Description : 从 cdrMsg 中倒叙读取长度为 length 的 buffer
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @param        length: input param
         * @return       {bool} 读取是否成功
         */
        static bool ReadDataReversed(CDRMessage &cdrMsg, octet *value, uint32_t length);

        /**
         * @Description : 从 cdrMsg 中读取 uint16_t
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadUint16(CDRMessage &cdrMsg, uint16_t &value);

        /**
         * @Description : 从 cdrMsg 中读取 EntityId
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadEntityId(CDRMessage &cdrMsg, EntityId &value);

        /**
         * @Description : 从 cdrMsg 中读取 SequenceNumber
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadSequenceNumber(CDRMessage &cdrMsg, SequenceNumber &value);

        /**
         * @Description : 从 cdrMsg 中读取 int32_t
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadInt32(CDRMessage &cdrMsg, int32_t &value);

        /**
         * @Description : 从 cdrMsg 中读取 uint32_t
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadUint32(CDRMessage &cdrMsg, uint32_t &value);

        /**
         * @Description : 从 cdrMsg 中读取 Guid
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadGUID(CDRMessage &cdrMsg, BaoSky::rtps::Guid &value);

        /**
         * @Description : 从 cdrMsg 中读取 Locator
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadLocator(CDRMessage &cdrMsg, Locator &value);

        /**
         * @Description : 从 cdrMsg 中读取 ProtocolVersion
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadProtocolVersion(CDRMessage &cdrMsg, ProtocolVersion &value);

        /**
         * @Description : 从 cdrMsg 中读取 VendorId_t
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadVendorId(CDRMessage &cdrMsg, VendorId_t &value);

        /**
         * @Description : 从 cdrMsg 中读取 ParameterList
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadParameterList(CDRMessage &cdrMsg, ParameterList &value);

        /**
         * @Description : 从 cdrMsg 中读取 Duration
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadDuration(CDRMessage &cdrMsg, Duration &value);

        /**
         * @Description : 从 cdrMsg 中读取 string
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadString(CDRMessage &cdrMsg, std::string &value);

        // TODO: UNIT TEST

        static bool AddFragmentNumberSet(CDRMessage &cdrMsg, FragmentNumberSet element);

        static bool ReadFragmentNumberSet(CDRMessage &cdrMsg, FragmentNumberSet &value);
        /**
         * @Description : 追加 SequenceNumberSet 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddSequenceNumberSet(CDRMessage &cdrMsg, SequenceNumberSet element);

        /**
         * @Description : 从 cdrMsg 中读取 SequenceNumberSet
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadSequenceNumberSet(CDRMessage &cdrMsg, SequenceNumberSet &value);

        /**
         * @Description : 追加 ChangeCount 到 cdrMsg 中
         * @param        cdrMsg: output param
         * @param        element: input param
         * @return       {bool} 追加是否成功
         */
        static bool AddChangeCount(CDRMessage &cdrMsg, ChangeCount element);

        /**
         * @Description : 从 cdrMsg 中读取 ChangeCount
         * @param        cdrMsg: input/output param 既是 input 参数，也修改了内容
         * @param        value: output param
         * @return       {bool} 读取是否成功
         */
        static bool ReadChangeCount(CDRMessage &cdrMsg, ChangeCount &value);

        static bool ReadGUIDPrefix(CDRMessage &cdrMsg, GuidPrefix &value);
        static bool AddTimestamp(CDRMessage &cdrMsg, Timestamp element);
        static bool ReadTimestamp(CDRMessage &cdrMsg, Timestamp &value);
        static bool ReadFragmentNumber(CDRMessage &cdrMsg, FragmentNumber &value);
    };
}

#endif