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
        KeyValueSerializer(size_t alignment = 4);
        ~KeyValueSerializer();

        bool serialize(std::string schema, const std::string ModelName, std::unordered_map<std::string, char *> &data);
        const char* buffer() const;

    private:
        bool serializeKeyValuePair(const parser::TreeNode &memberInfo, const std::string &key, const char* value);
        void delBuffer() {delete[] buffer_; buffer_ = nullptr; return;}

    private:
        char* buffer_;
        std::map<std::string, parser::ModelDefine> modelDefines_;
        std::map<std::string, size_t> offsetMap_;
        size_t ALIGNMENT_;
    };

} // namespace ngvs
} // namespace dsf

#endif