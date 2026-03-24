/*
 * @Author       : baihaoran 948942547@qq.com
 * @Date         : 2025-05-16 16:17:26
 * @FilePath     : /TXDDS/include/RTPS/DynamicTypeBuilder/XMLParser.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef _XML_DYNAMIC_PARSER_H_
#define _XML_DYNAMIC_PARSER_H_

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

#include "txdds/DCPS/dynamicType/DynamicTypeBuilder.h"
#include "txdds/DCPS/dynamicType/DynamicTypeBuilderFactory.h"
#include "txdds/RTPS/dynamicTypeParser/XMLParserComm.h"

namespace tinyxml2
{
    class XMLElement;
    class XMLDocument;
} // namespace tinyxml2

namespace BaoSky::rtps::DynamicBuilder
{
    class BaseNode;

    class TXDDS_API XMLDynamicParser
    {
    public:
        /*
         * @Description:
         * @param {string} &filename
         * @param {unique_ptr<BaseNode>} &root
         * @return {*}
         */
        static XMLP_ret loadXML(const std::string &filename, std::unique_ptr<BaseNode> &root);
        /*
         * @Description:
         * @param {XMLDocument} &xmlDoc
         * @param {unique_ptr<BaseNode>} &root
         * @return {*}
         */
        static XMLP_ret loadXML(tinyxml2::XMLDocument &xmlDoc, std::unique_ptr<BaseNode> &root);
        /*
         * @Description:
         * @param {XMLElement} &types
         * @return {*}
         */
        static XMLP_ret loadXMLDynamicTypes(tinyxml2::XMLElement &types);

    protected:
        static XMLP_ret parseXML(tinyxml2::XMLDocument &xmlDoc, std::unique_ptr<BaseNode> &root);

        static XMLP_ret parseXMLDynamicTypes(tinyxml2::XMLElement &types);

        static XMLP_ret parseDynamicTypes(tinyxml2::XMLElement *p_root);
        
        static XMLP_ret parseXMLTypes(tinyxml2::XMLElement *p_root);
        
        static XMLP_ret parseXMLDynamicType(tinyxml2::XMLElement *p_root);

        static XMLP_ret parseXMLStructDynamicType(tinyxml2::XMLElement *p_root);

        static XMLP_ret parseXMLUnionDynamicType(tinyxml2::XMLElement *p_root);

        static XMLP_ret parseXMLEnumDynamicType(tinyxml2::XMLElement *p_root);

        static XMLP_ret parseXMLAliasDynamicType(tinyxml2::XMLElement *p_root);

        static XMLP_ret parseXMLBitsetDynamicType(tinyxml2::XMLElement *p_root);

        static XMLP_ret parseXMLBitmaskDynamicType(tinyxml2::XMLElement *p_root);

        static std::shared_ptr<BaoSky::dds::MemberDescriptor> parseXMLMemberDynamicType(
            tinyxml2::XMLElement *p_root,
            BaoSky::dds::DynamicTypeBuilder *p_dynamictype,
            uint32_t mId);

        static std::shared_ptr<BaoSky::dds::MemberDescriptor> parseXMLMemberDynamicType(
            tinyxml2::XMLElement *p_root,
            BaoSky::dds::DynamicTypeBuilder *p_dynamictype,
            uint32_t mId,
            const std::string &values);
    };
}

#endif