/*
 * @Author       : tangfuqi tangfuqi@baosight.com
 * @Date         : 2025-05-13 17:47:05
 * @FilePath     : /TXDDS/include/DCPS/dynamicType/TypeObjectUtils.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#pragma once
#include <list>
#include <tuple>
#include <utility>
#include <vector>

#include "txdds/DCPS/dynamicType/CommonType.h"
namespace BaoSky::dds
{
    class TypeObject;
    class TypeIdentifier;

    void GetTypeIdentifierHash(TypeObject &obj, TypeIdentifier &identitifer);
    bool AddStructMember(TypeObject *obj, const std::tuple<std::string, std::string, uint32_t> &memberdescriptor, MemberId memberId);
    bool AddStructMember(TypeObject *obj, const std::tuple<std::string, std::string, uint32_t, std::vector<uint32_t>, std::string> &memberdescriptor, MemberId memberId);
    std::pair<TypeIdentifier *, TypeObject *> CreateStructType(const std::string &type, const std::list<std::tuple<std::string, std::string, uint32_t, std::vector<uint32_t>, std::string>> &typeDescriber);

    // customTypeName    typeName type  length  1 dimision
    //  Added by tangfuqi@baosight.com on 2025-06-12 10:47:30
    //  std::pair<TypeIdentifier *, TypeObject *> CreateArrayType(const std::string &type, std::tuple<std::string, std::string, uint32_t> &typeDescriber);

    std::pair<TypeIdentifier *, TypeObject *> CreateStructType(const std::string &type, const std::list<std::tuple<std::string, std::string, uint32_t>> &typeDescriber);

    std::pair<TypeIdentifier *, TypeObject *> CreateArrayType(const std::string &type, std::tuple<std::string, std::vector<uint32_t>> &typeDescriber);
    std::pair<TypeIdentifier *, TypeObject *> CreateSequenceType(const std::string &type, std::tuple<std::string, uint32_t> &typeDescriber);

}