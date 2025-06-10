/**
 * @author DSF Team
 * @file ModelParser.h
 * @date 2025-04-16
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @brief 
 */

#ifndef _MODEL_PARSER_H_
#define _MODEL_PARSER_H_

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <boost/property_tree/ptree.hpp>

namespace dsf
{
namespace parser
{
    static const std::unordered_map<std::string, size_t> basicTypeSizes = {
        {"int32", 4}, {"float32", 4}, {"int64", 8}, {"string", 82}, {"int16", 2}, {"int8", 1}};

    typedef enum {
        MODEL_PARSER_OK = 0,
        ERROR_MODEL_PARSE_FAILED,
    } error_code_t;

    // 新增树节点结构
    struct TreeNode {
        std::string name;               // 成员名称（基本类型：纯名称；非基本类型：name:version）
        std::string type;               // 类型（int32, nonBasic 等）
        size_t size;                    // 大小
        unsigned int offset;            // 偏移量
        std::vector<TreeNode> children; // 子节点（仅非基本类型或数组/序列）
        std::string version;            // 版本（仅非基本类型）
        std::string nonBasicTypeName;   // 非基本类型名称（如果适用）
        bool is_array;                  // 是否为数组元素
        std::vector<int> array_indices; // 数组索引（如果适用）

        TreeNode() : size(0), offset(0), is_array(false) {}
    };

    struct ModelDefine {
        std::string modelName;
        std::string modelVersion;
        std::string schema;
        size_t size;
        std::vector<TreeNode> members; // 替换 mapKeyType 和 members
    };

    class ModelParser
    {
    public:
        error_code_t parseSchema(std::map<std::string, ModelDefine> &modelDefines,
                                 const std::string &schema, std::string &errorMsg);
        bool findNodeAndGetLeaves(const ModelDefine &model, const std::string &targetName,
                                  std::vector<TreeNode> &leaves);
        bool findNodeAllLeaves(const ModelDefine &model, std::vector<TreeNode> &leaves);
        void printmembersInfo(std::vector<TreeNode> &nodes);
        void printAllLeafNodesInfo(const ModelDefine &model) const;

    private:
        void getLeafNodes(const TreeNode &node, std::vector<TreeNode> &leaves);
        std::set<std::string> visiting;

        std::string child2xml(const boost::property_tree::ptree &childNode,
                              const std::string &rootName);
        void resolveModelMembers(const std::string &currentModelNameAndVersion,
                                 std::map<std::string, ModelDefine> &allNodes,
                                 std::map<std::string, boost::property_tree::ptree> &structNodes,
                                 std::vector<TreeNode> &currentModelMembers, size_t &modelSize,
                                 size_t &offset, const std::string &modelVersion);
        size_t getBasicTypeSize(const std::string &type);
    };
} // namespace 
} // namespace dsf

#endif // MODEL_PARSER_H