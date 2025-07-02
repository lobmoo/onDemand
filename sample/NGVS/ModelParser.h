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
#include <mutex>
#include <boost/property_tree/ptree.hpp>

namespace dsf
{
namespace parser
{
    /*DT_BOOLEAN: 布尔，1字节（FALSE=0, TRUE=1）
    DT_BYTE: 8位，1字节（16#00~16#FF）
    DT_WORD: 16位，2字节（16#0000~16#FFFF）
    DT_DWORD: 32位，4字节（16#0000_0000~16#FFFF_FFFF）
    DT_LWORD: 64位，8字节（16#0000_0000_0000_0000~16#FFFF_FFFF_FFFF_FFFF）
    DT_SINT: 8位有符号整数，1字节（-128~127）
    DT_USINT: 8位无符号整数，1字节（0~255）
    DT_INT: 16位有符号整数，2字节（-32768~32767）
    DT_UINT: 16位无符号整数，2字节（0~65535）
    DT_DINT: 32位有符号整数，4字节（-2147483648~2147483647）
    DT_UDINT: 32位无符号整数，4字节（0~4_294_967_295）
    DT_LINT: 64位有符号整数，8字节（-2^63~2^63-1）
    DT_ULINT: 64位无符号整数，8字节（0~2^64-1）
    DT_REAL: 32位浮点数，4字节（IEEE 754单精度）
    DT_LREAL: 64位浮点数，8字节（IEEE 754双精度）
    DT_CHAR: 单字节字符，1字节
    DT_CHARSEQ: 单字节字符数组，占用[N+2]字节，N≤254，默认N=80（即82字节）
    DT_STRING: 字符串（未明确具体大小，需参考实现）
    DT_STRINGSEQ: 字符串数组（未明确具体大小，需参考实现）
    DT_WCHAR: 宽字节字符，2字节
    DT_WCHARSEQ: 宽字节字符数组，占用[N+2]字（即2*(N+2)字节），N≤16382，默认N=254（即508字节）
    DT_WSTRING: 宽字符串（未明确具体大小，需参考实现）
    DT_WSTRINGSEQ: 宽字符串数组（未明确具体大小，需参考实现）*/

    static const std::unordered_map<std::string, size_t> basicTypeSizes = {
        {"DT_BOOLEAN", 1},    // 布尔，1字节
        {"DT_BYTE", 1},       // 8位，1字节
        {"DT_WORD", 2},       // 16位，2字节
        {"DT_DWORD", 4},      // 32位，4字节
        {"DT_LWORD", 8},      // 64位，8字节
        {"DT_SINT", 1},       // 8位有符号整数，1字节
        {"DT_USINT", 1},      // 8位无符号整数，1字节
        {"DT_INT", 2},        // 16位有符号整数，2字节
        {"DT_UINT", 2},       // 16位无符号整数，2字节
        {"DT_DINT", 4},       // 32位有符号整数，4字节
        {"DT_UDINT", 4},      // 32位无符号整数，4字节
        {"DT_LINT", 8},       // 64位有符号整数，8字节
        {"DT_ULINT", 8},      // 64位无符号整数，8字节
        {"DT_REAL", 4},       // 32位浮点数，4字节
        {"DT_LREAL", 8},      // 64位浮点数，8字节
        {"DT_CHAR", 1},       // 单字节字符，1字节
        {"DT_CHARSEQ", 82},   // 单字节字符数组，默认N=80，占82字节
        {"DT_STRING", 82},    // 字符串，假设与DT_CHARSEQ相同
        {"DT_WCHAR", 2},      // 宽字节字符，2字节
        {"DT_WCHARSEQ", 508}, // 宽字节字符数组，默认N=254，占508字节
        {"DT_WSTRING", 508}   // 宽字符串，假设与DT_WCHARSEQ相同
        //ADD
    };

    typedef enum {
        MODEL_PARSER_OK = 0,
        ERROR_MODEL_PARSE_FAILED,
    } error_code_t;

    // 新增树节点结构
    struct TreeNode {
        std::string name;    // 成员名称（基本类型：纯名称；非基本类型：name:version）
        std::string type;    // 类型（int32, nonBasic 等）
        size_t size;         // 大小
        unsigned int offset; // 偏移量
        std::vector<std::shared_ptr<TreeNode>> children; // 子节点（仅非基本类型或数组/序列）
        std::string version;                             // 版本（仅非基本类型）
        std::string nonBasicTypeName;                    // 非基本类型名称（如果适用）
        bool is_array;                                   // 是否为数组元素
        std::vector<int> array_indices;                  // 数组索引（如果适用）

        TreeNode() : size(0), offset(0), is_array(false) {}
    };

    struct ModelDefine {
        std::string modelName;
        std::string modelVersion;
        std::string schema;
        size_t size;
        std::vector<std::shared_ptr<TreeNode>> members; //根节点们
    };

    bool forwardToBuffer(const std::string &type, const std::string &value,
                         std::vector<uint8_t> &buffer);
    std::string forwardToString(const std::vector<uint8_t> &buffer, const std::string &type);

    class ModelParser
    {
    public:
        bool init(const std::string &schema, std::string &errorMsg)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return parseSchema(modelDefines_, schema, errorMsg) == MODEL_PARSER_OK;
        }

        bool findNodeAndGetLeaves(const ModelDefine &model, const std::string &targetName,
                                  std::vector<std::shared_ptr<dsf::parser::TreeNode>> &leaves);
        bool findNodeAllLeaves(const ModelDefine &model,
                               std::vector<std::shared_ptr<dsf::parser::TreeNode>> &leaves);

        const std::unordered_map<std::string, ModelDefine> getModelDefines() const
        {
            return modelDefines_;
        }

        void set_alignment(size_t alignment)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ALIGNMENT_ = alignment;
        }

        static void printmembersInfo(std::vector<std::shared_ptr<dsf::parser::TreeNode>> &nodes);
        static void printAllLeafNodesInfo(const ModelDefine &model);

        static ModelParser &getInstance(size_t alignment = 4)
        {
            static ModelParser instance(alignment);
            return instance;
        }

    private:
        ModelParser(size_t alignment);
        ~ModelParser() = default;
        ModelParser(const ModelParser &) = delete;
        ModelParser &operator=(const ModelParser &) = delete;

        void getLeafNodes(const TreeNode &node,
                          std::vector<std::shared_ptr<dsf::parser::TreeNode>> &leaves);

        std::string child2xml(const boost::property_tree::ptree &childNode,
                              const std::string &rootName);
        error_code_t parseSchema(std::unordered_map<std::string, ModelDefine> &modelDefines,
                                 const std::string &schema, std::string &errorMsg);
        void resolveModelMembers(const std::string &currentModelNameAndVersion,
                                 std::unordered_map<std::string, ModelDefine> &allNodes,
                                 std::vector<std::shared_ptr<TreeNode>> &currentModelMembers,
                                 size_t &modelSize, size_t &offset, const std::string &modelVersion,
                                 const std::string &parentName = "");
        void resolveModelMembers_ex(const std::string &currentModelNameAndVersion,
                                    std::unordered_map<std::string, ModelDefine> &allNodes,
                                    std::vector<std::shared_ptr<TreeNode>> &currentModelMembers,
                                    size_t &modelSize, size_t &offset,
                                    const std::string &modelVersion, const std::string &parentName);
        size_t getBasicTypeSize(const std::string &type);

        void updateChildNames(TreeNode &node, const std::string &parentPrefix);

    private:
        size_t ALIGNMENT_;
        std::set<std::string> visiting;
        std::vector<std::string> doParseModels; // 需要解析的模型列表1
        std::unordered_map<std::string, ModelDefine> modelDefines_;
        std::map<std::string, boost::property_tree::ptree> structNodes_;
        std::mutex mutex_;
    };
} // namespace parser
} // namespace dsf

#endif // MODEL_PARSER_H