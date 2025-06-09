/**
 * @file NgvsSerialize.cpp
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-05-28
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par �?改日�?:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-05-28     <td>1.0     <td>wwk   <td>�?�??
 * </table>
 */
#include "NgvsSerialize.h"
#include "log/logger.h"
#include <cmath>

namespace dsf
{
namespace ngvs
{

    NgvsSerializer::NgvsSerializer(size_t alignment) : ALIGNMENT_(alignment)
    {
    }

    bool NgvsSerializer::serialize(std::string schema, const std::string ModelName)
    {
        ModelParser parser;
        std::string error_message;

        /*1.解析xml数据*/
        error_code_t ret = parser.parseSchema(modelDefines_, schema, error_message);
        if (ret != dsf::ngvs::MODEL_PARSER_OK) {
            LOG(error) << "parse schema failed, ret: " << ret;
            return false;
        }

        /*2.解析model下面的数�?类型*/
        auto it = modelDefines_.find(ModelName);
        if (it == modelDefines_.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }

        // /*找到数据类型，开始�?�找根节点的数据类型以及计算偏移*/
        ModelDefine modelDefine = it->second;
        LOG(info) << "----------------------------/n";
        LOG(info) << "model name: " << modelDefine.modelName
                  << "  model name: " << modelDefine.modelVersion
                  << "  model size: " << modelDefine.size;
        for(auto &ptr : modelDefine.members)
        {
            LOG(info) << "ele name: " << ptr.name << "  ele type: " << ptr.type
                      << "  ele size: " << ptr.size << "  ele offset: " << ptr.offset;
        }
        // for (auto &ele : modelDefine.mapKeyType) {
        //     LOG(info) << "ele name: " << ele.first << "  ele type: " << ele.second.type
        //               << "  ele size: " << ele.second.size << "  ele offset: " << ele.second.offset;
        // }
        LOG(info) << "----------------------------/n";

        // std::vector<Result> res = calculateStructSize(modelDefine.mapKeyType);
        // for (const auto &result : res) {
        //     LOG(info) << "+++++name: " << result.name << ", type: " << result.type
        //               << ", size: " << result.size << ", offset: " << result.offset;
        // }

        return 0;
    }

    std::vector<Result>
    NgvsSerializer::calculateStructSize(const std::map<std::string, std::string> &data)
    {
        // // 按根节点分组
        // std::map<std::string, std::vector<std::pair<std::string, std::string>>> root_nodes;
        // for (const auto &entry : data) {
        //     std::string key = entry.first;
        //     std::string root =
        //         key.substr(0, key.find('[') != std::string::npos ? key.find('[') : key.find('.'));
        //     root_nodes[root].push_back(entry);
        // }
        // for (auto &node : root_nodes) {
        //     // LOG(debug) << "root: " << node.first;
        // }

        // std::vector<Result> result;
        // for (const auto &root_entry : root_nodes) {
        //     std::string root = root_entry.first;
        //     const auto &entries = root_entry.second;

        //     if (entries[0].first.find('[') == std::string::npos
        //         && entries[0].first.find('.') == std::string::npos) {
        //         // �?不是结构也不�?数组，直接�?�理简单变�?
        //         std::string name = root;
        //         std::string dtype = entries[0].second;
        //         size_t size = basicTypeSizes.at(dtype);
        //         unsigned int offset = size; // 简单变量的偏移量等于其大小
        //         result.push_back({name, dtype, size, offset});
        //     } else {

        //         std::vector<std::pair<std::string, std::string>> first_elem_keys;
        //         for (const auto &entry : entries) {
        //             // �?收集 [0] 的键（数组）或包�? . 的键（平铺结构体�?
        //             if ((entry.first.find("[0]") != std::string::npos)
        //                 || (entry.first.find('.') != std::string::npos
        //                     && entry.first.find('[') == std::string::npos)) {
        //                 first_elem_keys.push_back(entry);
        //             }
        //         }
        //         if (first_elem_keys.empty())
        //             continue;

        //         // 处理简单数�?
        //         if (entries[0].first.find('[') != std::string::npos
        //             && first_elem_keys[0].first.find('.') == std::string::npos) {
        //             std::string name = root;
        //             std::string dtype = first_elem_keys[0].second;
        //             size_t size = basicTypeSizes.at(dtype);
        //             // 数组总大�? = 元素大小 * 元素�?�?
        //             int array_size =
        //                 std::count_if(entries.begin(), entries.end(),
        //                               [&root](const auto &e) {
        //                                   return e.first.find(root) != std::string::npos;
        //                               })
        //                 * size;
        //             unsigned int offset = array_size; // 数组的偏移量等于总大小（已�?�齐�?
        //             result.push_back({name, dtype, size, offset});
        //         } else {
        //             // 复杂结构体数组（�? long_array2�?
        //             // 按成员分�?
        //             std::map<std::string, std::vector<std::pair<std::string, std::string>>>
        //                 struct_members;
        //             for (const auto &entry : first_elem_keys) {
        //                 std::string member = entry.first.substr(entry.first.find("[0].") + 4);
        //                 member = member.substr(0, member.find('.'));
        //                 /*存到struct 的数组里�?*/
        //                 struct_members[member].push_back(entry);
        //             }
        //             // 计算结构体大小和偏移量（考虑四字节�?�齐�?
        //             unsigned int offset = 0;
        //             for (const auto &member_entry : struct_members) {
        //                 std::string member = member_entry.first;
        //                 const auto &members = member_entry.second;
        //                 int size;
        //                 std::string dtype;

        //                 if (members.size() == 1
        //                     && members[0].first.substr(members[0].first.find("[0].") + 4).find('.')
        //                            == std::string::npos) {
        //                     // 简单成员（�? long_array2[0].first�?
        //                     dtype = members[0].second;
        //                     size = basicTypeSizes.at(dtype);
        //                 } else {
        //                     // 嵌�?�结构体（�?? complex_member6�?
        //                     dtype = member;
        //                     size = 0;
        //                     unsigned int sub_offset = 0;
        //                     std::vector<std::pair<std::string, std::string>> sorted_members =
        //                         members;
        //                     std::sort(sorted_members.begin(), sorted_members.end());
        //                     for (const auto &sub_entry : sorted_members) {
        //                         int sub_size = basicTypeSizes.at(sub_entry.second);
        //                         // 四字节�?�齐
        //                         if (sub_offset % 4 != 0) {
        //                             sub_offset += 4 - (sub_offset % 4);
        //                         }
        //                         sub_offset += sub_size;
        //                         size += sub_size; // �?加实际大�?
        //                     }
        //                     // 嵌�?�结构体偏移量四字节对齐
        //                     sub_offset = static_cast<unsigned int>(
        //                         std::ceil(static_cast<double>(sub_offset) / 4) * 4);
        //                     size = sub_offset; // 嵌�?�结构体大小按�?�齐后的偏移量�?�算
        //                 }

        //                 // 四字节�?�齐
        //                 if (offset % 4 != 0) {
        //                     offset += 4 - (offset % 4);
        //                 }
        //                 offset += size;
        //                 // 记录�?一�?成员
        //                 if (offset == size || (offset - size) % 4 == 0) {
        //                     result.push_back({root, dtype, basicTypeSizes.at(dtype), offset});
        //                 }
        //                 break; // �?记录�?一�?成员
        //             }
        //         }
        //     }
        // }

        //return result;
    }

    const std::vector<char> &NgvsSerializer::buffer() const
    {
        return buffer_;
    }

    size_t NgvsSerializer::alignOffset(size_t offset, size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    NgvsSerializer::~NgvsSerializer()
    {
    }

} // namespace ngvs
} // namespace dsf
