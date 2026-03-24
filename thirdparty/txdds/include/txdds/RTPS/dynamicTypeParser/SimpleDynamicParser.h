/*
 * @Author       : baihaoran 948942547@qq.com
 * @Date         : 2025-05-16 16:17:26
 * @FilePath     : /TXDDS/include/RTPS/DynamicTypeBuilder/XMLParser.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef _SIMPLE_DYNAMIC_PARSER_H_
#define _SIMPLE_DYNAMIC_PARSER_H_

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

#include "txdds/DCPS/dynamicType/DynamicTypeBuilder.h"
#include "txdds/DCPS/dynamicType/DynamicTypeBuilderFactory.h"
#include "txdds/RTPS/dynamicTypeParser/SimpleTypeStructure/TypeStructureParser.h"

namespace BaoSky::rtps::DynamicBuilder
{
    class BaseNode;

    class TXDDS_API SimpleDynamicParser
    {
    public:
        /*
         * @Description:
         * @param {string} &filename
         * @param {unique_ptr<BaseNode>} &root
         * @return {*}
         */
        static int64_t loadSimpleTypeStructure(const std::string &filename, std::unique_ptr<BaseNode> &root);
        /*
         * @Description:
         * @param {XMLDocument} &xmlDoc
         * @param {unique_ptr<BaseNode>} &root
         * @return {*}
         */
        static int64_t loadSimpleTypeStructure(TypeStructureParser &parser, std::unique_ptr<BaseNode> &root);

    protected:
        static int64_t parseSimpleTypeStructure(TypeStructureParser &parser, std::unique_ptr<BaseNode> &root);

        static int64_t parseDynamicTypes(TypeStructureParser *parser);

        static int64_t parseType(StructInfo *info);

        static int64_t parseStructTypeInfo(StructInfo *info);

        static std::shared_ptr<BaoSky::dds::MemberDescriptor> parseMemberTypeInfo(
            StructMemberInfo *memberInfo,
            BaoSky::dds::DynamicTypeBuilder *p_dynamictype,
            uint32_t mId);

        static std::shared_ptr<BaoSky::dds::MemberDescriptor> parseMemberTypeInfo(
            StructMemberInfo *memberInfo,
            BaoSky::dds::DynamicTypeBuilder *p_dynamictype,
            uint32_t mId,
            const std::string &values);
    };
}

#endif