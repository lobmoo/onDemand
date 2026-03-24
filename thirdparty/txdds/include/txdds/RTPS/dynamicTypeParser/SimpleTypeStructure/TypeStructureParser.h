/*
 * @Author       : baihaoran 948942547@qq.com
 * @Date         : 2025-06-17 10:58:26
 * @FilePath     : /TXDDS/include/RTPS/dynamicTypeParser/SimpleTypeStructure/TypeStructureParser.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef _TYPE_STRUCTURE_PARSER_H_
#define _TYPE_STRUCTURE_PARSER_H_
#include "txdds/RTPS/dynamicTypeParser/SimpleTypeStructure/TypeStructure.h"

namespace BaoSky::rtps::DynamicBuilder
{
    extern const std::string MAGIC_STR;

    class TypeStructureParser
    {
    public:
        TypeStructureParser() {}
        ~TypeStructureParser()
        {
            ClearBuilder();
        }

        bool InitStructsTable(std::string encoding, std::string version);
        void ClearBuilder();

        StructInfo *CreatStructInfo();
        StructInfo *CreatStructTypeInfo();
        bool WriteFile(const std::string &filepath);
        std::string Dump();
        // StructMemberInfo *CreatMemberInfo(std::string name, std::string type, std::string arrayRange = "", std::string nonBasicTypeName = "");
        // bool InsertMemberInfo(StructInfo *parent, StructMemberInfo *member);

        bool LoadFile(const std::string &filepath);
        bool LoadBuffer(std::string &dumpStr);
        std::vector<StructInfo *> GetStructTypeInfos();
        StructInfo *GetMainStructInfo();

    public:
        size_t GetSize();
        std::string &GetMagicStr()
        {
            return magicStr;
        }

    private:
        virtual bool WriteStructInfo(int level, std::ostringstream &file, StructInfo *info);
        bool WriteMemberInfo(int level, std::ostringstream &file, StructMemberInfo *info);
        // bool ParseFile();
        void skipWhiteSpace();
        bool ParseValue(std::istringstream &buffer);
        bool ParseStructInfo(std::istringstream &buffer);
        bool ParseMemberInfo(std::istringstream &buffer, StructInfo *parent);
        bool ParseHeader(std::string &str);
        std::pair<std::string, std::string> ParseKeyValue(std::string &str);
        std::string Trim(const std::string &str);
        std::string ToLowerCase(std::string s);

    private:
        std::string magicStr;
        StructsTable mNgvsTable;
        std::string line{};
        size_t pos{};
    };
}
#endif