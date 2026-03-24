/*
 * @Author       : tangfuqi tangfuqi@baosight.com
 * @Date         : 2025-04-28 16:09:13
 * @FilePath     : /TXDDS/include/DCPS/dynamicType/TypeNamesGenerator.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#pragma once
#include <string>
#include <vector>
#include <sstream>
namespace BaoSky::dds
{

    class TypeNamesGenerator
    {
    public:
        static std::string get_array_type_name(std::string type_name, std::vector<uint32_t> bound);
        static std::string get_sequence_type_name(const std::string &type_name, uint32_t bound);
        static std::string get_string_type_name(uint32_t bound, bool wide);
        static std::string get_map_type_name(const std::string &key_type_name, const std::string &value_type_name, uint32_t bound);
    };

}