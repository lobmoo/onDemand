/**
 * @file KeyValueSerialize.h
 * @brief 
 * @author wqb (894424179@.com)
 * @version 1.0
 * @date 2025-06-09
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-06-09     <td>1.0     <td>wqb   <td>初版
 * </table>
 */

#ifndef KEYVALUE_SERIALIZE_H
#define KEYVALUE_SERIALIZE_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "ModelParser.h"
#include "unordered_map"

namespace dsf
{
namespace ngvs
{

    class KeyValueSerializer
    {
    public:
        static KeyValueSerializer &getInstance()
        {
            static KeyValueSerializer instance;
            return instance;
        }
        ~KeyValueSerializer();
        bool serialize(const std::string &schema, const std::string &ModelName, 
                       const std::unordered_map<std::string, char *> &data, std::vector<char> &outBuffer);
        bool deserialize(const std::string &schema, const std::string &ModelName,
                         const std::vector<char> &inBuffer, std::unordered_map<std::string, char *> &outData);
    private:
        KeyValueSerializer(size_t alignment = 4);
        bool serializeKeyValuePair(parser::TreeNode &leaf, const std::string &key, const char* value, 
                                   const parser::ModelDefine &modelDefine, std::vector<char> &outBuffer);

    private:
        std::map<std::string, parser::ModelDefine> modelDefines_;
        size_t ALIGNMENT_;
    };

} // namespace ngvs
} // namespace dsf

#endif