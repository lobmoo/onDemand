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
    NgvsSerializer::NgvsSerializer(size_t alignment) : ALIGNMENT_(alignment)
    {
    }

    inline bool NgvsSerializer::map2Buffer(const ModelDefine &model,
                                           const std::unordered_map<std::string, char *> &inData,
                                           std::vector<char> &outBuffer)
    {
        outBuffer.clear();
        outBuffer.reserve(model.size);
        size_t offset = 0;
        for(const auto &member : model.members) {
            auto it = inData.find(member.name);
            if (it == inData.end()) {
                LOG(warning) << "Member not found in input data: " << member.name;
                size_t memberSize = member.size;
                offset = alignOffset(offset, ALIGNMENT_);
                std::memcpy(outBuffer.data() + offset, "/0", memberSize);
                offset += memberSize;
                continue; // Skip if member not found in input data
            }

            size_t memberSize = member.size;
            offset = alignOffset(offset, ALIGNMENT_);
            std::memcpy(outBuffer.data() + offset, it->second, memberSize);
            offset += memberSize;
        }
        return true;
    }

    bool NgvsSerializer::serialize(const std::string &schema, const std::string &ModelName,
                                   const std::unordered_map<std::string, char *> &inData,
                                   std::vector<char> &outBuffer)
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

        // /*找到数据类型*/
        ModelDefine model = it->second;

#ifdef NGVS_DEBUG
        parser.printAllLeafNodesInfo(model);
        parser.printmembersInfo(model.members);
#endif
        /*按照大小对udt进行排序,并且结构体放到最前面*/

        std::stable_sort(model.members.begin(), model.members.end(),
                  [](const TreeNode &a, const TreeNode &b) {
                      bool a_is_nonbasic = a.type == "nonBasic";
                      bool b_is_nonbasic = b.type == "nonBasic";

                      if (a_is_nonbasic != b_is_nonbasic) {
                          return a_is_nonbasic > b_is_nonbasic;
                      }
                      if (a_is_nonbasic && b_is_nonbasic) {
                          return a.size > b.size;
                      }
                      return false;
                  });

        if(map2Buffer(model, inData, outBuffer)) {
            LOG(info) << "Serialization successful, buffer size: " << outBuffer.size();
        } else {
            LOG(error) << "Serialization failed";
            return false;
        }     
        return true;
    }
    bool NgvsSerializer::serialize(const std::string &schema, const std::string &ModelName,
                                   const std::vector<char> &inBuffer, std::vector<char> &outBuffer)
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

        // /*找到数据类型*/
        ModelDefine model = it->second;

#ifdef NGVS_DEBUG
        parser.printAllLeafNodesInfo(model);
#endif
        /*按照大小对udt进行排序,并且结构体放到最前面*/

        std::stable_sort(model.members.begin(), model.members.end(),
                  [](const TreeNode &a, const TreeNode &b) {
                      bool a_is_nonbasic = a.type == "nonBasic";
                      bool b_is_nonbasic = b.type == "nonBasic";

                      if (a_is_nonbasic != b_is_nonbasic) {
                          return a_is_nonbasic > b_is_nonbasic;
                      }
                      if (a_is_nonbasic && b_is_nonbasic) {
                          return a.size > b.size;
                      }
                      return false;
                  });

        // std::vector<TreeNode> leaves;
        // parser.findNodeAndGetLeaves(model, "long_array", leaves);
        // for(auto &leaf : leaves) {
        //     LOG(info) << "Leaf Node: " << leaf.name << ", Type: " << leaf.type
        //               << ", Size: " << leaf.size << ", Offset: " << leaf.offset;
        // }

        parser.printmembersInfo(model.members);

        return true;
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
