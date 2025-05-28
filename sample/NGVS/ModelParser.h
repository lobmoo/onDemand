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
#include <set>

namespace dsf
{
namespace ngvs
{
    typedef enum
    {
        MODEL_PARSER_OK = 0,
        ERROR_MODEL_PARSE_FAILED,



    }model_parser_error_code_t;


    struct ModelDefine {
        std::string modelName;
        std::string modelVersion;
        std::string schema;
        std::map<std::string, std::string> mapKeyType;
    };

    class ModelParser
    {
    public:
        model_parser_error_code_t parseSchema(std::map<std::string, ModelDefine> &modelDefines, const std::string &schema, std::string &errorMsg);
    private:
        std::set<std::string> visiting; //记录每次递归的模型名称，防止死循环
        std::set<std::string> _parsedExternalModels;

        std::string child2xml(const boost::property_tree::ptree &childNode,
                              const std::string &rootName);
        void resolveModelMembers(const std::string &modelName,
                                 std::map<std::string, ModelDefine> &allModels,
                                 std::map<std::string, boost::property_tree::ptree> &structNodes,
                                 std::map<std::string, std::string> &currentModelMemebers);
        void generateArrayKeys(const std::string &memberName, const std::string &memberType,
                               const std::vector<int> &dimensions, std::vector<int> currentIndices,
                               std::map<std::string, std::string> &currentModelMemebers);
        void generateSequenceKeys(const std::string &memberName, const std::string &memberType,
                                  int maxLength,
                                  std::map<std::string, std::string> &currentModelMemebers);
        void generateArrayMembers(const std::string &memberName, const std::vector<int> &dimensions,
                                  std::vector<int> currentIndices,
                                  const std::map<std::string, std::string> &subMembers,
                                  std::map<std::string, std::string> &currentModelMembers);
    };
} // namespace ac
} // namespace dsf
#endif