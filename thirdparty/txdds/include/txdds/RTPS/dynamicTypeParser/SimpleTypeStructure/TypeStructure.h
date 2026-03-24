/*
 * @Author       : baihaoran 948942547@qq.com
 * @Date         : 2025-06-17 10:57:51
 * @FilePath     : /TXDDS/include/RTPS/dynamicTypeParser/SimpleTypeStructure/TypeStructure.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef _TYPE_STRUCTURE_H_
#define _TYPE_STRUCTURE_H_

#include <map>
#include <vector>
#include <string>
#include <fstream>

namespace BaoSky::rtps::DynamicBuilder
{

    class StructMemberInfo
    {
    public:
        void SetName(std::string name);
        void SetType(std::string type);
        void SetArrayDimensions(std::string arrayDimension);
        void SetNonBasicTypeName(std::string nonBasicTypeName);
        bool SetAttribute(std::string key, std::string value);

        std::string &GetName();
        std::string &GetType();
        std::string &GetArrayDimensions();
        std::string &GetNonBasicTypeName();
        const char *GetAttribute(std::string key);

        std::vector<std::pair<std::string, std::string>> &GetAttributes();

    public:
        std::string mname{};
        std::string mtype{};
        // uint32_t offset{};
        // std::string length{};
        std::string marrayDimensions{};                                 // array
        std::string mnonBasicTypeName{};                                // udt
        std::vector<std::pair<std::string, std::string>> mAttributes{}; // attributes
    };

    class StructInfo
    {
    public:
        std::string mHashName; // hash后的strInput
        std::vector<StructMemberInfo *> mMembers{};
        uint32_t mTotalLen;

        StructInfo() {};
        ~StructInfo()
        {
            mHashName.clear();
            mTotalLen = 0;
            for (auto &p : mMembers)
            {
                delete p;
                p = nullptr;
            }
            mMembers.clear();
        }

        StructMemberInfo *CreatMemberInfo();
        StructMemberInfo *CreatMemberInfo(std::string name, std::string type, std::string arrayRange, std::string nonBasicTypeName);

        std::vector<StructMemberInfo *>& GetMemberInfos()
        {
            return mMembers;
        }
    };

    struct StructsTable
    {
        std::string mEncoding;
        std::string mVersion; // hash后的strInput
        StructInfo *mStruct{};
        std::vector<StructInfo *> mTypes{};
    };

}
#endif