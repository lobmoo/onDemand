/*
 * @Author: songwenguang 563734@baosight.com
 * @Date: 2025-04-02 11:32:34
 * @FilePath: /TXDDS/include/DCPS/dynamicType/DynamicData.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description:
 */
#pragma once
#include <map>

#include "txdds/DCPS/common/ReturnCode.h"
#include "txdds/DCPS/dynamicType/DynamicType.h"

#include "txcdr/Cdr.h"
#include "txcdr/DeserializeCdr.h"
#include "txcdr/SerializeCdr.h"
using ReturnCode = BaoSky::dds::ReturnCode;

namespace BaoSky::dds
{
    class TXDDS_API DynamicData
    {
    public:
        DynamicData(std::shared_ptr<DynamicType> dynamicType);
        ~DynamicData();
        DynamicData(const DynamicData &) = delete;
        DynamicData &operator=(const DynamicData &) = delete;
        MemberId GetMemberIdByName(const std::string &name);
        MemberId GetMemberIdByIndex(uint32_t index);
        uint32_t GetItemCount();
        uint32_t GetSequenceBound();
        bool Equals(const DynamicData *other);
        ReturnCode ClearAllValues();
        ReturnCode ClearNonkeyValues();
        ReturnCode ClearValue(MemberId id);
        ReturnCode ClearPrimitiveData(TypeKind kind, std::map<MemberId, void *>::iterator iterator);
        ReturnCode ClearSequenceData(TypeKind eleTypeKind);
        ReturnCode ClearMapData();
        DynamicData *LoanValue(MemberId id);
        ReturnCode ReturnLoanedValue(DynamicData *value);
        DynamicData *Clone();
        ReturnCode AddPrimitiveTypeToValues(TypeKind kind, MemberId memberId);
        ReturnCode AddSeqTypeToValues(TypeKind kind, MemberId memberId);
        bool IsComplexType(TypeKind kind);
        ReturnCode AddDynamicMapData(MemberId keyId, MemberId valueId);
        ReturnCode AddDynamicSequenceData(MemberId &valueId);
        ReturnCode AddSeqElementToValue(TypeKind eleTypeKind, MemberId elementId);

        ReturnCode GetBoolValue(bool &value, MemberId id);
        ReturnCode GetByteValue(octet &value, MemberId id);
        ReturnCode GetInt8Value(int8_t &value, MemberId id);
        ReturnCode GetUint8Value(uint8_t &value, MemberId id);
        ReturnCode GetInt16Value(int16_t &value, MemberId id);
        ReturnCode GetUint16Value(uint16_t &value, MemberId id);
        ReturnCode GetInt32Value(int32_t &value, MemberId id);
        ReturnCode GetUint32Value(uint32_t &value, MemberId id);
        ReturnCode GetInt64Value(int64_t &value, MemberId id);
        ReturnCode GetUint64Value(uint64_t &value, MemberId id);
        ReturnCode GetFloat32Value(float &value, MemberId id);
        ReturnCode GetFloat64Value(double &value, MemberId id);
        ReturnCode GetFloat128Value(long double &value, MemberId id);
        ReturnCode GetChar8Value(char &value, MemberId id);
        ReturnCode GetChar16Value(wchar_t &value, MemberId id);
        ReturnCode GetStringValue(std::string &value, MemberId id);
        ReturnCode GetWstringValue(std::wstring &value, MemberId id);
        ReturnCode GetCompositeTypeValue(DynamicData **value, MemberId id);

        ReturnCode GetBoolSequenceValue(std::vector<bool> &value, MemberId id);
        ReturnCode GetByteSequenceValue(std::vector<octet> &value, MemberId id);
        ReturnCode GetInt8SequenceValue(std::vector<int8_t> &value, MemberId id);
        ReturnCode GetUint8SequenceValue(std::vector<uint8_t> &value, MemberId id);
        ReturnCode GetInt16SequenceValue(std::vector<int16_t> &value, MemberId id);
        ReturnCode GetUint16SequenceValue(std::vector<uint16_t> &value, MemberId id);
        ReturnCode GetInt32SequenceValue(std::vector<int32_t> &value, MemberId id);
        ReturnCode GetUint32SequenceValue(std::vector<uint32_t> &value, MemberId id);
        ReturnCode GetInt64SequenceValue(std::vector<int64_t> &value, MemberId id);
        ReturnCode GetUint64SequenceValue(std::vector<uint64_t> &value, MemberId id);
        ReturnCode GetFloat32SequenceValue(std::vector<float> &value, MemberId id);
        ReturnCode GetFloat64SequenceValue(std::vector<double> &value, MemberId id);
        ReturnCode GetFloat128SequenceValue(std::vector<long double> &value, MemberId id);
        ReturnCode GetChar8SequenceValue(std::vector<char> &value, MemberId id);
        ReturnCode GetChar16SequenceValue(std::vector<wchar_t> &value, MemberId id);
        ReturnCode GetStringSequenceValue(std::vector<std::string> &value, MemberId id);
        ReturnCode GetWstringSequenceValue(std::vector<std::wstring> &value, MemberId id);
        template <typename Type>
        ReturnCode GetPrimitiveTypeValue(Type &value, MemberId id);
        template <typename Type>
        ReturnCode GetMapPrimitiveTypeValue(Type &value, std::map<MemberId, void *>::iterator iterator);
        template <typename Type>
        ReturnCode GetTypeValue(Type &value, MemberId id);
        template <typename Type>
        ReturnCode GetSeqPrimitiveTypeValue(Type &value, MemberId id);
        template <typename Type>
        ReturnCode GetSeqTypeValue(Type &value, MemberId id);

        ReturnCode SetBoolValue(bool value, MemberId id);
        ReturnCode SetByteValue(octet value, MemberId id);
        ReturnCode SetInt8Value(int8_t value, MemberId id);
        ReturnCode SetUint8Value(uint8_t value, MemberId id);
        ReturnCode SetInt16Value(int16_t value, MemberId id);
        ReturnCode SetUint16Value(uint16_t value, MemberId id);
        ReturnCode SetInt32Value(int32_t value, MemberId id);
        ReturnCode SetUint32Value(uint32_t value, MemberId id);
        ReturnCode SetInt64Value(int64_t value, MemberId id);
        ReturnCode SetUint64Value(uint64_t value, MemberId id);
        ReturnCode SetFloat32Value(float value, MemberId id);
        ReturnCode SetFloat64Value(double value, MemberId id);
        ReturnCode SetFloat128Value(long double value, MemberId id);
        ReturnCode SetChar8Value(char value, MemberId id);
        ReturnCode SetChar16Value(wchar_t value, MemberId id);
        ReturnCode SetStringValue(std::string value, MemberId id);
        ReturnCode SetWstringValue(std::wstring value, MemberId id);
        ReturnCode SetCompositeTypeValue(DynamicData **value, MemberId id);

        ReturnCode SetBoolSequenceValue(const std::vector<bool> &value, MemberId id);
        ReturnCode SetByteSequenceValue(const std::vector<octet> &value, MemberId id);
        ReturnCode SetInt8SequenceValue(const std::vector<int8_t> &value, MemberId id);
        ReturnCode SetUint8SequenceValue(const std::vector<uint8_t> &value, MemberId id);
        ReturnCode SetInt16SequenceValue(const std::vector<int16_t> &value, MemberId id);
        ReturnCode SetUint16SequenceValue(const std::vector<uint16_t> &value, MemberId id);
        ReturnCode SetInt32SequenceValue(const std::vector<int32_t> &value, MemberId id);
        ReturnCode SetUint32SequenceValue(const std::vector<uint32_t> &value, MemberId id);
        ReturnCode SetInt64SequenceValue(const std::vector<int64_t> &value, MemberId id);
        ReturnCode SetUint64SequenceValue(const std::vector<uint64_t> &value, MemberId id);
        ReturnCode SetFloat32SequenceValue(const std::vector<float> &value, MemberId id);
        ReturnCode SetFloat64SequenceValue(const std::vector<double> &value, MemberId id);
        ReturnCode SetFloat128SequenceValue(const std::vector<long double> &value, MemberId id);
        ReturnCode SetChar8SequenceValue(const std::vector<char> &value, MemberId id);
        ReturnCode SetChar16SequenceValue(const std::vector<wchar_t> &value, MemberId id);
        ReturnCode SetStringSequenceValue(const std::vector<std::string> &value, MemberId id);
        ReturnCode SetWstringSequenceValue(const std::vector<std::wstring> &value, MemberId id);
        template <typename Type>
        ReturnCode SetPrimitiveTypeValue(Type value, MemberId id);
        template <typename Type>
        ReturnCode SetMapPrimitiveTypeValue(Type value, std::map<MemberId, void *>::iterator iterator);
        template <typename Type>
        ReturnCode SetTypeValue(Type value, MemberId id);
        template <typename Type>
        ReturnCode SetSeqPrimitiveTypeValue(const std::vector<Type> &value, MemberId id);
        template <typename Type>
        ReturnCode SetSeqTypeValue(const Type &value, MemberId id);

        ReturnCode SerializeData(BaoSky::Cdr::SerializeCdr &sCdr);
        ReturnCode SerializeSeqData(BaoSky::Cdr::SerializeCdr &sCdr, TypeKind eleTypeKind, size_t maxEleNums);
        ReturnCode SerializeSequenceData(BaoSky::Cdr::SerializeCdr &sCdr, TypeKind eleTypeKind);
        template <typename Type>
        ReturnCode SerializeVecBasicTypeData(const std::vector<Type> &vec, BaoSky::Cdr::SerializeCdr &sCdr);
        template <typename Type>
        ReturnCode SerializeArrayBasicTypeData(const std::vector<Type> &vec, BaoSky::Cdr::SerializeCdr &sCdr);
        ReturnCode SerializeMapData(BaoSky::Cdr::SerializeCdr &sCdr, TypeKind eleTypeKind, std::map<MemberId, void *>::iterator iterator);
        ReturnCode DeserializeData(BaoSky::Cdr::DeserializeCdr &dCdr);
        ReturnCode DeserializeSeqData(BaoSky::Cdr::DeserializeCdr &dCdr, TypeKind eleTypeKind, size_t maxEleNums);
        ReturnCode DeserializeSequenceData(BaoSky::Cdr::DeserializeCdr &dCdr, TypeKind eleTypeKind, size_t maxEleNums);
        ReturnCode DeserializeMapData(BaoSky::Cdr::DeserializeCdr &dCdr, TypeKind eleTypeKind, MemberId id);
        template <typename Type>
        ReturnCode DeserializeVecBasicTypeData(std::vector<Type> *&vec, BaoSky::Cdr::DeserializeCdr &dCdr, size_t maxEleNums);
        template <typename Type>
        ReturnCode DeserializeArrayBasicTypeData(std::vector<Type> *&vec, BaoSky::Cdr::DeserializeCdr &dCdr, size_t maxEleNums);
        uint32_t CalculateSerializedSize(BaoSky::Cdr::CdrSizeCalculator &cdrSizeCalculator, size_t &currentAlignment);
        uint32_t CalculateSeqSerializedSize(BaoSky::Cdr::CdrSizeCalculator &cdrSizeCalculator, size_t &currentAlignment, TypeKind eleTypeKind, size_t maxEleNums);
        uint32_t CalculateSequenceSerializedSize(BaoSky::Cdr::CdrSizeCalculator &cdrSizeCalculator, size_t &currentAlignment, TypeKind eleTypeKind);
        uint32_t CalculateMapSerializedSize(BaoSky::Cdr::CdrSizeCalculator &cdrSizeCalculator, size_t &currentAlignment, TypeKind eleTypeKind, std::map<MemberId, void *>::iterator iterator);
        uint32_t CalculateSeqMaxSerializedSize(BaoSky::Cdr::CdrSizeCalculator &cdrSizeCalculator, size_t &currentAlignment, TypeKind eleTypeKind);
        uint32_t CalculateMaxSerializedSize(BaoSky::Cdr::CdrSizeCalculator &cdrSizeCalculator, size_t &currentAlignment);

    private:
        std::shared_ptr<DynamicType> mDynamicType;
        std::map<MemberId, void *> mValues;
        std::vector<MemberId> mLoanedValues;
    };
}