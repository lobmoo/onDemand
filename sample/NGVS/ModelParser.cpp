/**
 * @author DSF Team
 * @file ModelParser.cpp
 * @date 2025-04-16
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @brief 
 */

#include "ModelParser.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include "log/logger.h"

namespace dsf
{
namespace ngvs
{
    std::string ModelParser::child2xml(const boost::property_tree::ptree &childNode,
                                       const std::string &rootName)
    {
        std::stringstream ss;
        boost::property_tree::ptree root;
        root.push_back(std::make_pair(rootName, childNode));
        boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
        boost::property_tree::write_xml(ss, root, settings);
        std::string xmlString = ss.str();

        size_t pos = xmlString.find("?>");
        if (pos != std::string::npos) {
            xmlString = xmlString.substr(pos + 2);
        }

        std::string startTag = "<" + rootName + ">";
        std::string endTag = "</" + rootName + ">";
        size_t startPos = xmlString.find(startTag);
        size_t endPos = xmlString.find(endTag);

        if (startPos != std::string::npos && endPos != std::string::npos) {
            xmlString = xmlString.substr(startPos + startTag.length(),
                                         endPos - (startPos + startTag.length()));
        }

        return xmlString;
    }

    model_parser_error_code_t ModelParser::parseSchema(
        std::map<std::string, ModelDefine> &modelDefines, const std::string &schema,
        std::string &errorMsg)
    {
       // _externalModelSchemas = modelSchemas;
        std::istringstream iss(schema);
        boost::property_tree::ptree ptInput;
        try {
            boost::property_tree::read_xml(iss, ptInput);
        } catch (const boost::property_tree::xml_parser::xml_parser_error &e) {
            errorMsg = e.what();
            return ERROR_MODEL_PARSE_FAILED;
        }

        std::map<std::string, boost::property_tree::ptree> structNodes;

        // 首先处理输入的 schema 中的模型
        for (const auto &modelNode : ptInput.get_child("models")) {
            if (modelNode.first == "struct") {
                ModelDefine model;
                model.modelName = modelNode.second.get<std::string>("<xmlattr>.name");
                model.modelVersion = modelNode.second.get<std::string>("<xmlattr>.version");

                model.schema = child2xml(modelNode.second, "struct");

                modelDefines[model.modelName + ":" + model.modelVersion] = model;
                structNodes[model.modelName + ":" + model.modelVersion] = modelNode.second;
            }
        }

        // 再次遍历所有模型，处理继承和 nonBasic 类型
        for (auto it = modelDefines.begin(); it != modelDefines.end(); ++it) {
            const std::string &modelNameAndVersion = it->first;
            ModelDefine &modelDefine = it->second;
            resolveModelMembers(modelNameAndVersion, modelDefines, structNodes,
                                modelDefine.mapKeyType);
        }

        return MODEL_PARSER_OK;
    }

    void ModelParser::resolveModelMembers(
        const std::string &currentModelNameAndVersion, std::map<std::string, ModelDefine> &allNodes,
        std::map<std::string, boost::property_tree::ptree> &structNodes,
        std::map<std::string, std::string> &currentModelMemebers)
    {
        if (visiting.count(currentModelNameAndVersion)) {
            LOG(error) << "Detected cyclic dependency at model: " << currentModelNameAndVersion;
            return;
        }
        visiting.insert(currentModelNameAndVersion);
        const auto &itStructNode = structNodes.find(currentModelNameAndVersion);
        if (itStructNode == structNodes.end()) {
            // // 尝试从 _externalModelSchemas 中加载
            // if (_externalModelSchemas.count(currentModelNameAndVersion)
            //     && _parsedExternalModels.find(currentModelNameAndVersion)
            //            == _parsedExternalModels.end()) {
            //     LOG(info) << "Loading external model schema for model: "
            //               << currentModelNameAndVersion;
            //     std::string schema =
            //         _externalModelSchemas.at(currentModelNameAndVersion).data.dataSchema();
            //     std::istringstream ssExternal(schema);
            //     boost::property_tree::ptree ptExternal;
            //     try {
            //         boost::property_tree::read_xml(ssExternal, ptExternal);
            //         // 直接处理 ptExternal，假设它是 <struct> 节点
            //         if (ptExternal.get_child_optional("struct")) { // 如果根节点下有 struct
            //             const auto &externalStructNode = ptExternal.get_child("struct");
            //             ModelDefine externalModel;
            //             externalModel.modelName =
            //                 externalStructNode.get<std::string>("<xmlattr>.name");
            //             externalModel.modelVersion =
            //                 externalStructNode.get<std::string>("<xmlattr>.version");

            //             std::stringstream ssSchema;
            //             boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
            //             boost::property_tree::write_xml(ssSchema, externalStructNode, settings);
            //             externalModel.schema = ssSchema.str();

            //             allNodes[externalModel.modelName + ":" + externalModel.modelVersion] =
            //                 externalModel;
            //             structNodes[externalModel.modelName + ":" + externalModel.modelVersion] =
            //                 externalStructNode;
            //             _parsedExternalModels.insert(externalModel.modelName + ":"
            //                                          + externalModel.modelVersion);
            //             // 重新查找刚刚加载的模型节点
            //             resolveModelMembers(currentModelNameAndVersion, allNodes, structNodes,
            //                                 currentModelMemebers);
            //             return; // 找到并处理后返回
            //         } else {
            //             LOG(error)
            //                 << "Illegal schema for external model: " + currentModelNameAndVersion;
            //         }
            //     } catch (const boost::property_tree::xml_parser::xml_parser_error &e) {
            //         LOG(error) << "Error parsing external schema for model: "
            //                    << currentModelNameAndVersion << " - " << e.what();
            //         return;
            //     }
            // }
            LOG(warning) << "Failed to find struct node for model: " + currentModelNameAndVersion;
            return; // 找不到对应的 struct 节点
        }
        const auto &structNode = itStructNode->second;

        // 处理 baseType
        boost::optional<std::string> baseTypeNameOptional =
            structNode.get_optional<std::string>("<xmlattr>.baseType");
        boost::optional<std::string> baseTypeVersionOptional =
            structNode.get_optional<std::string>("<xmlattr>.baseTypeVersion");

        if (baseTypeNameOptional && baseTypeVersionOptional) {
            resolveModelMembers(baseTypeNameOptional.get() + ":" + baseTypeVersionOptional.get(),
                                allNodes, structNodes, currentModelMemebers);
        } else if (baseTypeNameOptional) {
            LOG(warning) << "baseType '" << baseTypeNameOptional.get()
                         << "' has no version specified in model: " << currentModelNameAndVersion;
            // 可以考虑是否需要报错或使用默认版本，这里先记录 warning
        }

        // 处理当前 struct 的 members
        for (const auto &memberNode : structNode.get_child("")) {
            if (memberNode.first == "member") {
                std::string memberName = memberNode.second.get<std::string>("<xmlattr>.name");
                std::string memberType = memberNode.second.get<std::string>("<xmlattr>.type");
                boost::optional<std::string> arrayDimensionsOptional =
                    memberNode.second.get_optional<std::string>("<xmlattr>.arrayDimensions");
                boost::optional<std::string> sequenceMaxLengthOptional =
                    memberNode.second.get_optional<std::string>("<xmlattr>.sequenceMaxLength");

                if (arrayDimensionsOptional) {
                    std::vector<int> dimensions;
                    std::vector<std::string> dimsStr;
                    boost::split(dimsStr, arrayDimensionsOptional.get(), boost::is_any_of(","));
                    for (const auto &dimStr : dimsStr) {
                        try {
                            dimensions.push_back(std::stoi(dimStr));
                        } catch (const std::invalid_argument &e) {
                            LOG(error) << "Invalid array dimension: " << dimStr
                                       << " in model: " << currentModelNameAndVersion
                                       << ", member: " << memberName;
                            continue; // Or handle error appropriately
                        } catch (const std::out_of_range &e) {
                            LOG(error) << "Array dimension out of range: " << dimStr
                                       << " in model: " << currentModelNameAndVersion
                                       << ", member: " << memberName;
                            continue; // Or handle error appropriately
                        }
                    }
                    if (memberType != "nonBasic") {
                        generateArrayKeys(memberName, memberType, dimensions, {},
                                          currentModelMemebers);
                    } else {
                        std::string nonBasicTypeName =
                            memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName");
                        auto nonBasicTypeVersionOptional =
                            memberNode.second.get_optional<std::string>("<xmlattr>.version");

                        if (!nonBasicTypeName.empty() && nonBasicTypeVersionOptional) {
                            std::map<std::string, std::string> nonBasicMembers;
                            resolveModelMembers(nonBasicTypeName + ":"
                                                    + nonBasicTypeVersionOptional.get(),
                                                allNodes, structNodes, nonBasicMembers);

                            std::function<void(std::vector<int>)> generateKeys;
                            generateKeys = [&](std::vector<int> indices) {
                                if (indices.size() == dimensions.size()) {
                                    std::string prefix = memberName;
                                    for (int idx : indices) {
                                        prefix += "[" + std::to_string(idx) + "]";
                                    }
                                    for (const auto &kv : nonBasicMembers) {
                                        currentModelMemebers[prefix + "." + kv.first] = kv.second;
                                    }
                                    return;
                                }
                                int dim = dimensions[indices.size()];
                                for (int i = 0; i < dim; ++i) {
                                    auto next = indices;
                                    next.push_back(i);
                                    generateKeys(next);
                                }
                            };
                            generateKeys({});
                        } else {
                            LOG(error)
                                << "nonBasic array member '" << memberName << "' of type '"
                                << nonBasicTypeName << "' has no version specified in model: "
                                << currentModelNameAndVersion;
                        }

                        // LOG(error) << "ArrayDimensions is not supported for nonBasic type member: "
                        //            << memberName << " in model: " << currentModelNameAndVersion;
                        // Optionally handle this case, e.g., by resolving the nonBasic type's members as usual
                    }
                } else if (sequenceMaxLengthOptional) {
                    if (memberType != "nonBasic") {
                        try {
                            int maxLength = std::stoi(sequenceMaxLengthOptional.get());
                            if (maxLength > 0) {
                                generateSequenceKeys(memberName, memberType, maxLength,
                                                     currentModelMemebers);
                            } else {
                                LOG(error)
                                    << "Invalid sequenceMaxLength (must be > 0): " << maxLength
                                    << " in model: " << currentModelNameAndVersion
                                    << ", member: " << memberName;
                            }
                        } catch (const std::invalid_argument &e) {
                            LOG(error) << "Invalid sequenceMaxLength format: "
                                       << sequenceMaxLengthOptional.get()
                                       << " in model: " << currentModelNameAndVersion
                                       << ", member: " << memberName;
                        } catch (const std::out_of_range &e) {
                            LOG(error) << "sequenceMaxLength out of range: "
                                       << sequenceMaxLengthOptional.get()
                                       << " in model: " << currentModelNameAndVersion
                                       << ", member: " << memberName;
                        }
                    } else {
                        //TODO: 复杂结构sequenceMaxLength暂不支持
                        LOG(error)
                            << "sequenceMaxLength is not supported for nonBasic type member: "
                            << memberName << " in model: " << currentModelNameAndVersion;
                        // Optionally handle this case
                    }
                } else if (memberType == "nonBasic") {
                    std::string nonBasicTypeName =
                        memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName");
                    boost::optional<std::string> nonBasicTypeVersionOptional =
                        memberNode.second.get_optional<std::string>("<xmlattr>.version");

                    if (!nonBasicTypeName.empty() && nonBasicTypeVersionOptional.is_initialized()) {
                        std::map<std::string, std::string> nonBasicMembers;
                        resolveModelMembers(nonBasicTypeName + ":"
                                                + nonBasicTypeVersionOptional.get(),
                                            allNodes, structNodes, nonBasicMembers);
                        for (auto it = nonBasicMembers.begin(); it != nonBasicMembers.end(); ++it) {
                            const std::string &key = it->first;
                            const std::string &value = it->second;
                            currentModelMemebers[memberName + "." + key] = value;
                        }
                    } else {
                        LOG(error) << "nonBasic member '" << memberName << "' of type '"
                                   << nonBasicTypeName << "' has no version specified in model: "
                                   << currentModelNameAndVersion;
                        // 这里需要处理 nonBasic 类型缺少版本的情况，可以返回错误或者记录错误信息
                    }
                } else {
                    currentModelMemebers[memberName] = memberType;
                }
            }
        }
        visiting.erase(currentModelNameAndVersion);
    }

    void ModelParser::generateArrayKeys(const std::string &memberName,
                                        const std::string &memberType,
                                        const std::vector<int> &dimensions,
                                        std::vector<int> currentIndices,
                                        std::map<std::string, std::string> &currentModelMemebers)
    {
        if (currentIndices.size() == dimensions.size()) {
            std::string key = memberName;
            for (int index : currentIndices) {
                key += "[" + std::to_string(index) + "]";
            }
            currentModelMemebers[key] = memberType;
            return;
        }

        int currentDimension = dimensions[currentIndices.size()];
        for (int i = 0; i < currentDimension; ++i) {
            std::vector<int> nextIndices = currentIndices;
            nextIndices.push_back(i);
            generateArrayKeys(memberName, memberType, dimensions, nextIndices,
                              currentModelMemebers);
        }
    }

    void ModelParser::generateSequenceKeys(const std::string &memberName,
                                           const std::string &memberType, int maxLength,
                                           std::map<std::string, std::string> &currentModelMemebers)
    {
        for (int i = 0; i < maxLength; ++i) {
            currentModelMemebers[memberName + "[" + std::to_string(i) + "]"] = memberType;
        }
    }

} // namespace ac
} // namespace dsf
