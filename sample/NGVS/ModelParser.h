/**
 * @author DSF Team
 * @file ModelParser.h
 * @date 2025-04-16
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @brief 
 */

#ifndef _MODEL_PARSER_H_
#define _MODEL_PARSER_H_

#include <boost/property_tree/ptree.hpp>
#include <map>
#include <unordered_map>
#include <set>

namespace dsf
{
namespace ngvs
{

    static const std::unordered_map<std::string, size_t> basicTypeSizes = {
        {"int32", 4}, {"float32", 4}, {"int64", 8}, {"string", 82}, {"int16", 2}, {"int8", 1}};

    typedef enum {
        MODEL_PARSER_OK = 0,
        ERROR_MODEL_PARSE_FAILED,
    } error_code_t;

    struct Result {
        std::string name;
        std::string type;
        size_t size;
        unsigned int offset;
    };

    struct ModelDefine {
        std::string modelName;
        std::string modelVersion;
        std::string schema;
        size_t size;
        std::map<std::string, Result>
            mapKeyType; //后续可以优化为unordered_map，提升查找效率
    };

    class ModelParser
    {
    public:
        error_code_t parseSchema(std::map<std::string, ModelDefine> &modelDefines,
                                 const std::string &schema, std::string &errorMsg);

    private:
        std::set<std::string> visiting; //记录每次递归的模型名称，防止死循环
        std::set<std::string> _parsedExternalModels;

        std::string child2xml(const boost::property_tree::ptree &childNode,
                              const std::string &rootName);
        void resolveModelMembers(const std::string &currentModelNameAndVersion,
                                 std::map<std::string, ModelDefine> &allNodes,
                                 std::map<std::string, boost::property_tree::ptree> &structNodes,
                                 std::map<std::string, Result> &currentModelMembers,
                                 size_t &modelSize, size_t &offset);
        // void generateArrayKeys(const std::string &memberName, const std::string &memberType,
        //                        const std::vector<int> &dimensions, std::vector<int> currentIndices,
        //                        std::map<std::string, std::string> &currentModelMemebers);
        // void generateSequenceKeys(const std::string &memberName, const std::string &memberType,
        //                           int maxLength,
        //                           std::map<std::string, std::string> &currentModelMemebers);
        // void generateArrayMembers(const std::string &memberName, const std::vector<int> &dimensions,
        //                           std::vector<int> currentIndices,
        //                           const std::map<std::string, std::string> &subMembers,
        //                           std::map<std::string, std::string> &currentModelMembers);
        void generateArrayKeys(const std::string &memberName, const std::string &memberType,
                               const std::vector<int> &dimensions, std::vector<int> currentIndices,
                               std::map<std::string, Result> &currentModelMembers,
                               size_t &offset);
        void generateSequenceKeys(const std::string &memberName, const std::string &memberType,
                                  int maxLength,
                                  std::map<std::string, Result> &currentModelMembers,
                                  size_t &offset);
        size_t getBasicTypeSize(const std::string &type);
    };
} // namespace ngvs
} // namespace dsf
#endif