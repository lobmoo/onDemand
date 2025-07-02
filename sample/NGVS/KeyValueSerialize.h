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
namespace kvpair
{

    class KeyValueSerializer
    {
    public:
        KeyValueSerializer(size_t alignment = 4);
        ~KeyValueSerializer();
        bool serialize(const std::string &ModelName, const std::unordered_map<std::string, std::string> &data, std::vector<char> &outBuffer);
        bool deserialize(const std::string &ModelName, const std::vector<char> &inBuffer, std::unordered_map<std::string, std::string> &outData);
    private:
        size_t alignOffset(size_t offset, size_t alignment);

    private:
        size_t ALIGNMENT_;
    };

} // namespace kvpair
} // namespace dsf

#endif