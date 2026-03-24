/*
 * @Author       : baihaoran 948942547@qq.com
 * @Date         : 2025-05-16 13:32:50
 * @FilePath     : /TXDDS/include/RTPS/DynamicTypeBuilder/ProfileManager.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef _PROFILE_MANAGER_H_
#define _PROFILE_MANAGER_H_
#include <cstdio>
#include <map>
#include <string>
#include <mutex>

#include "txdds/RTPS/dynamicTypeParser/XMLDynamicParser.h"
#include "txdds/RTPS/dynamicTypeParser/SimpleDynamicParser.h"
#include "txdds/DCPS/dynamicType/DynamicTypeBuilder.h"

namespace BaoSky::rtps::DynamicBuilder
{
    class TypeStructureParser;
    class StructMemberInfo;
    class StructInfo;
    class TXDDS_API ProfileManager
    {
    public:
        /**
         * @brief  根据xml名称加载xml文件
         * @param  &filename {string}
         * @return {XMLP_ret}
         */
        static XMLP_ret LoadXMLFile(const std::string &filename);
        /**
         * @brief  根据xml文件加载xml结构
         * @param  &doc {XMLDocument}
         * @return {XMLP_ret}
         */
        static XMLP_ret LoadXMLNode(tinyxml2::XMLDocument &doc);
        /**
         * @brief  根据名称加载SimpleType结构
         * @param  &filename {string}
         * @return {uint64_t}
         */
        static uint64_t LoadSimpleTypeStructure(const std::string &filename);
        /**
         * @brief  根据TypeStructureParser加载SimpleType结构
         * @param  &parser {TypeStructureParser}
         * @return {uint64_t}
         */
        static uint64_t LoadSimpleTypeStructure(TypeStructureParser &parser);
        /**
         * @brief  往sDynamicTypes容器存入name,type键值对
         * @param  &typeName {string}
         * @param  *type {DynamicTypeBuilder}
         * @return {bool}
         */
        static bool InsertDynamicTypeByName(const std::string &typeName, BaoSky::dds::DynamicTypeBuilder *type);
        /**
         * @brief  根据传入的type名称删除已创建的DynamicTypeBuilder
         * @param  &typeName {string}
         * @return {XMLP_ret}
         */
        static XMLP_ret DeleteDynamicTypeBuilder(const std::string &typeName);
        /**
         * @brief 根据传入的type名称获取已创建的DynamicTypeBuilder
         * @param  &typeName {string}
         * @return {DynamicTypeBuilder*}
         */
        static BaoSky::dds::DynamicTypeBuilder *GetDynamicTypeByName(const std::string &typeName);
        /**
         * @brief 清理资源
         * @return {}
         */
        static void DeleteInstance();

    private:
        static std::mutex mMutex;
        // 存储DynamicTypeBuilder的容器
        static std::map<std::string, BaoSky::dds::DynamicTypeBuilder *> sDynamicTypes;
        // 存储XMLfile名称的容器
        static std::map<std::string, XMLP_ret> mXMLfiles;
    };

}

#endif