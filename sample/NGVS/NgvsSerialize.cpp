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

#define NGVS_DEBUG

namespace dsf
{
namespace ngvs
{

    using namespace dsf::parser;
    NgvsSerializer::NgvsSerializer() : ALIGNMENT_(4)
    {
    }

    inline bool
    NgvsSerializer::map2Buffer(const ModelDefine &model,
                               const std::unordered_map<std::string, std::string> &inData,
                               std::vector<char> &outBuffer)
    {
        auto &parser = ModelParser::getInstance();
        std::vector<std::shared_ptr<dsf::parser::TreeNode>> leaves;
        parser.findNodeAllLeaves(model, leaves);

        // 1. 计算总大小
        size_t total_size = 0;
        size_t bit_count = 0;
        bool in_bit_sequence = false;
        std::vector<size_t> bit_offsets; // 记录每个bit区的起始offset
        std::vector<size_t> bit_counts;  // 记录每个bit区的bit数量

        for (auto &leaf : leaves) {
            if (leaf->type == "DT_BIT") {
                if (!in_bit_sequence) {
                    in_bit_sequence = true;
                    bit_offsets.push_back(total_size);
                    bit_count = 0;
                }
                bit_count++;
            } else {
                if (in_bit_sequence) {
                    // 结束bit区，分配空间
                    total_size = alignOffset(total_size, ALIGNMENT_);
                    bit_counts.push_back(bit_count);
                    total_size += (bit_count + 7) / 8;
                    in_bit_sequence = false;
                    bit_count = 0;
                }
                total_size = alignOffset(total_size, ALIGNMENT_);
                total_size += leaf->size;
            }
        }
        // 最后收尾
        if (in_bit_sequence) {
            total_size = alignOffset(total_size, ALIGNMENT_);
            bit_counts.push_back(bit_count);
            total_size += (bit_count + 7) / 8;
        }

        outBuffer.clear();
        outBuffer.resize(total_size, 0);

        // 2. 序列化
        size_t offset = 0;
        size_t leaf_idx = 0;
        size_t bit_zone_idx = 0;
        while (leaf_idx < leaves.size()) {
            // 处理bit区
            if (leaves[leaf_idx]->type == "DT_BIT") {
                size_t bit_count = bit_counts[bit_zone_idx];
                size_t bit_bytes = (bit_count + 7) / 8;
                size_t bit_offset = bit_offsets[bit_zone_idx];
                uint8_t bit_packed = 0;
                for (size_t i = 0; i < bit_count; ++i, ++leaf_idx) {
                    auto &leaf = leaves[leaf_idx];
                    auto it = inData.find(leaf->name);
                    uint8_t val = (it != inData.end() && it->second == "1") ? 1 : 0;
                    size_t byte_idx = i / 8;
                    size_t bit_idx = i % 8;
                    outBuffer[bit_offset + byte_idx] |= (val << bit_idx);
                }
                offset = bit_offset + bit_bytes;
                bit_zone_idx++;
                continue;
            }
            // 处理普通成员
            auto &leaf = leaves[leaf_idx];
            offset = alignOffset(offset, ALIGNMENT_);
            size_t memberSize = leaf->size;
            std::vector<uint8_t> memberData;
            auto it = inData.find(leaf->name);
            if (it == inData.end()) {
                std::memset(outBuffer.data() + offset, 0, memberSize);
                offset += memberSize;
                ++leaf_idx;
                continue;
            }
            try {
                dsf::parser::forwardToBuffer(leaf->type, it->second, memberData);
                std::memcpy(outBuffer.data() + offset, memberData.data(), memberSize);
                offset += memberSize;
            } catch (const std::exception &e) {
                LOG(error) << "Error converting value for key: " << it->first
                           << ", Error: " << e.what();
                return false;
            }
            ++leaf_idx;
        }
        return true;
    }

    inline bool NgvsSerializer::buffer2Map(const ModelDefine &model,
                                           const std::vector<char> &inBuffer,
                                           std::unordered_map<std::string, std::string> &outData)
    {
        outData.clear();
        size_t offset = 0;
        auto &parser = ModelParser::getInstance();
        std::vector<std::shared_ptr<dsf::parser::TreeNode>> leaves;
        parser.findNodeAllLeaves(model, leaves);

        size_t leaf_idx = 0;
        size_t bit_zone_idx = 0;
        size_t ALIGNMENT_ = 4;

        while (leaf_idx < leaves.size()) {
            // 处理bit区
            if (leaves[leaf_idx]->type == "DT_BIT") {
                // 统计bit区长度
                size_t bit_count = 0;
                size_t start_idx = leaf_idx;
                while (leaf_idx < leaves.size() && leaves[leaf_idx]->type == "DT_BIT") {
                    ++bit_count;
                    ++leaf_idx;
                }
                size_t bit_bytes = (bit_count + 7) / 8;
                offset = alignOffset(offset, ALIGNMENT_);
                // 拆分bit区
                for (size_t i = 0; i < bit_count; ++i) {
                    size_t byte_idx = i / 8;
                    size_t bit_idx = i % 8;
                    uint8_t byte = inBuffer[offset + byte_idx];
                    std::string val = ((byte >> bit_idx) & 0x01) ? "1" : "0";
                    outData[leaves[start_idx + i]->name] = val;
                }
                offset += bit_bytes;
                continue;
            }
            // 处理普通成员
            auto &leaf = leaves[leaf_idx];
            offset = alignOffset(offset, ALIGNMENT_);
            size_t memberSize = leaf->size;
            if (offset + memberSize > inBuffer.size()) {
                LOG(error) << "Buffer overflow: offset + memberSize out of range";
                return false;
            }
            std::vector<uint8_t> memberData(inBuffer.data() + offset,
                                            inBuffer.data() + offset + memberSize);
            try {
                outData[leaf->name] = forwardToString(memberData, leaf->type);
            } catch (const std::exception &e) {
                LOG(error) << "Error converting value for key: " << leaf->name
                           << ", Error: " << e.what();
                return false;
            }
            offset += memberSize;
            ++leaf_idx;
        }
        return true;
    }

    void NgvsSerializer::NgvsModelSort(ModelDefine &model)
    { /*按照大小对udt进行排序,并且结构体放到最前面*/

        std::stable_sort(
            model.members.begin(), model.members.end(),
            [](const std::shared_ptr<TreeNode> &a, const std::shared_ptr<TreeNode> &b) {
                bool a_is_nonbasic = a->type == "nonBasic";
                bool b_is_nonbasic = b->type == "nonBasic";
                if (a_is_nonbasic != b_is_nonbasic) {
                    return a_is_nonbasic && !b_is_nonbasic;
                }
                if (a_is_nonbasic && b_is_nonbasic) {
                    return a->size > b->size; // 大的在前面
                }
                return false;
            });
    }

    bool NgvsSerializer::deserialize(const std::string &ModelName,
                                     const std::vector<char> &inBuffer,
                                     std::unordered_map<std::string, std::string> &outData)
    {
        auto &parser = ModelParser::getInstance();
        auto modelDefines = parser.getModelDefines();
        auto it = modelDefines.find(ModelName);
        if (it == modelDefines.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }
        // /*找到数据类型*/
        ModelDefine model = it->second;

        NgvsModelSort(model);
        parser.printmembersInfo(model.members);
        if (!buffer2Map(model, inBuffer, outData)) {
            LOG(error) << "Deserialization failed";
            return false;
        }
        return true;
    }

    bool NgvsSerializer::serialize(const std::string &ModelName,
                                   const std::unordered_map<std::string, std::string> &inData,
                                   std::vector<char> &outBuffer)
    {
        auto &parser = ModelParser::getInstance();
        auto modelDefines = parser.getModelDefines();
        /*解析model下面的数�?类型*/
        auto it = modelDefines.find(ModelName);
        if (it == modelDefines.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }
        // /*找到数据类型*/
        ModelDefine model = it->second;
        /*按照大小对udt进行排序,并且结构体放到最前面*/

        NgvsModelSort(model);
        parser.printmembersInfo(model.members);
        if (!map2Buffer(model, inData, outBuffer)) {
            LOG(error) << "Serialization failed";
            return false;
        }
        return true;
    }

    bool NgvsSerializer::serialize(const std::string &ModelName, const std::vector<char> &inBuffer,
                                   std::vector<char> &outBuffer)
    {

        return true;
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
