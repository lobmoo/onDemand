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
        outBuffer.clear();
        outBuffer.resize(model.size);

        size_t offset = 0;
        auto  &parser = ModelParser::getInstance();
        std::vector<std::shared_ptr<dsf::parser::TreeNode>> leaves;
        size_t memberSize = 0;
        std::vector<uint8_t> memberData;
        parser.findNodeAllLeaves(model, leaves);
        for (const auto &leaf : leaves) {
            LOG(info) << "Leaf Node: " << leaf->name << ", Type: " << leaf->type
                      << ", Size: " << leaf->size << ", Offset: " << leaf->offset;
        }
        for (auto &leaf : leaves) {
            if (offset + memberSize > outBuffer.size()) {
                LOG(error) << "Buffer overflow: offset + memberSize out of range" << ", offset: " << offset
                           << ", memberSize: " << memberSize << ", outBuffer.size(): " << outBuffer.size();
                return false;
            }
            auto it = inData.find(leaf->name);
            if (it == inData.end()) {
                LOG(warning) << "Member not found in input data: " << leaf->name;
                /*没有找到就填个空就继续*/
                size_t memberSize = leaf->size;
                offset = alignOffset(offset, ALIGNMENT_);
                std::memset(outBuffer.data() + offset, 0, memberSize);
                offset += memberSize;
                continue;
            }

            /*找到了就好好整*/
            memberSize = leaf->size;
            offset = alignOffset(offset, ALIGNMENT_);
            memberData.clear();
            try {
                dsf::parser::forwardToBuffer(leaf->type, it->second, memberData);
            } catch (const std::exception &e) {
                LOG(error) << "Error converting value for key: " << it->first
                           << ", Error: " << e.what();
                //todo 后续考虑是否继续
                return false;
            }
            std::memcpy(outBuffer.data() + offset, memberData.data(), memberSize);
            offset += memberSize;
        }
        return true;
    }

    inline bool NgvsSerializer::buffer2Map(const ModelDefine &model,
                                           const std::vector<char> &inBuffer,
                                           std::unordered_map<std::string, std::string> &outData)
    {
        outData.clear();
        std::vector<uint8_t> memberData;
        size_t offset = 0;
        size_t memberSize = 0;
        auto &parser = ModelParser::getInstance();
        std::vector<std::shared_ptr<dsf::parser::TreeNode>> leaves;
        parser.findNodeAllLeaves(model, leaves);
        for (const auto &leaf : leaves) {
            LOG(info) << "Leaf Node: " << leaf->name << ", Type: " << leaf->type
                      << ", Size: " << leaf->size << ", Offset: " << leaf->offset;
        }

        for (auto &leaf : leaves) {
            if (offset + memberSize > inBuffer.size()) {
                LOG(error) << "Buffer overflow: offset + memberSize out of range";
                return false;
            }

            memberSize = leaf->size;
            offset = alignOffset(offset, ALIGNMENT_);
            memberData.clear();
            memberData = std::vector<uint8_t>(inBuffer.data() + offset,
                                              inBuffer.data() + offset + leaf->size);
            try {
                outData[leaf->name] = forwardToString(memberData, leaf->type);
            } catch (const std::exception &e) {
                LOG(error) << "Error converting value for key: " << leaf->name
                           << ", Error: " << e.what();
                //todo 后续考虑是否继续
                return false;
            }
            offset += memberSize;
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
