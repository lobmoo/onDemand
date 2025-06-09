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

    error_code_t ModelParser::parseSchema(std::map<std::string, ModelDefine> &modelDefines,
                                          const std::string &schema, std::string &errorMsg)
    {
        std::istringstream iss(schema);
        boost::property_tree::ptree ptInput;
        try {
            boost::property_tree::read_xml(iss, ptInput);
        } catch (const boost::property_tree::xml_parser::xml_parser_error &e) {
            errorMsg = e.what();
            return ERROR_MODEL_PARSE_FAILED;
        }

        std::map<std::string, boost::property_tree::ptree> structNodes;

        for (const auto &modelNode : ptInput.get_child("models")) {
            if (modelNode.first == "struct") {
                ModelDefine model;
                model.modelName = modelNode.second.get<std::string>("<xmlattr>.name");
                model.modelVersion = modelNode.second.get<std::string>("<xmlattr>.version");
                model.schema = child2xml(modelNode.second, "struct");
                model.size = 0;
                modelDefines[model.modelName + ":" + model.modelVersion] = model;
                structNodes[model.modelName + ":" + model.modelVersion] = modelNode.second;
            }
        }

        for (auto it = modelDefines.begin(); it != modelDefines.end(); ++it) {
            const std::string &modelNameAndVersion = it->first;
            ModelDefine &modelDefine = it->second;
            size_t modelSize = 0;
            size_t offset = 0;
            resolveModelMembers(modelNameAndVersion, modelDefines, structNodes,
                                modelDefine.mapKeyType, modelDefine.members, modelSize, offset);
            modelDefine.size = (offset + 3) / 4 * 4; // Pad total size to multiple of 4
        }

        return MODEL_PARSER_OK;
    }

    void ModelParser::resolveModelMembers(
        const std::string &currentModelNameAndVersion, std::map<std::string, ModelDefine> &allNodes,
        std::map<std::string, boost::property_tree::ptree> &structNodes,
        std::map<std::string, Result> &currentModelMembers,
        std::vector<Result> &currentModelMembersVector, size_t &modelSize, size_t &offset)
    {
        if (visiting.count(currentModelNameAndVersion)) {
            LOG(error) << "Detected cyclic dependency at model: " << currentModelNameAndVersion;
            return;
        }
        visiting.insert(currentModelNameAndVersion);
        const auto &itStructNode = structNodes.find(currentModelNameAndVersion);
        if (itStructNode == structNodes.end()) {
            LOG(warning) << "Failed to find struct node for model: " + currentModelNameAndVersion;
            visiting.erase(currentModelNameAndVersion);
            return;
        }
        const auto &structNode = itStructNode->second;

        // Handle baseType
        auto baseTypeNameOptional = structNode.get_optional<std::string>("<xmlattr>.baseType");
        auto baseTypeVersionOptional =
            structNode.get_optional<std::string>("<xmlattr>.baseTypeVersion");
        if (baseTypeNameOptional && baseTypeVersionOptional) {
            resolveModelMembers(baseTypeNameOptional.get() + ":" + baseTypeVersionOptional.get(),
                                allNodes, structNodes, currentModelMembers,
                                currentModelMembersVector, modelSize, offset);
        } else if (baseTypeNameOptional) {
            LOG(warning) << "baseType '" << baseTypeNameOptional.get()
                         << "' has no version specified in model: " << currentModelNameAndVersion;
        }

        // Process members
        for (const auto &memberNode : structNode.get_child("")) {
            if (memberNode.first == "member") {
                std::string memberName = memberNode.second.get<std::string>("<xmlattr>.name");
                std::string memberType = memberNode.second.get<std::string>("<xmlattr>.type");
                auto arrayDimensionsOptional =
                    memberNode.second.get_optional<std::string>("<xmlattr>.arrayDimensions");
                auto sequenceMaxLengthOptional =
                    memberNode.second.get_optional<std::string>("<xmlattr>.sequenceMaxLength");

                if (arrayDimensionsOptional) {
                    std::vector<int> dimensions;
                    std::vector<std::string> dimsStr;
                    boost::split(dimsStr, arrayDimensionsOptional.get(), boost::is_any_of(","));
                    size_t arraySize = 1;
                    for (const auto &dimStr : dimsStr) {
                        try {
                            int dim = std::stoi(dimStr);
                            dimensions.push_back(dim);
                            arraySize *= dim;
                        } catch (const std::exception &e) {
                            LOG(error)
                                << "Invalid array dimension: " << dimStr << " - " << e.what();
                            continue;
                        }
                    }
                    offset = (offset + 3) / 4 * 4; // Align array start
                    if (memberType != "nonBasic") {
                        generateArrayKeys(memberName, memberType, dimensions, {},
                                          currentModelMembers, currentModelMembersVector, offset);
                    } else {
                        std::string nonBasicTypeName =
                            memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName");
                        auto nonBasicTypeVersionOptional =
                            memberNode.second.get_optional<std::string>("<xmlattr>.version");
                        if (!nonBasicTypeName.empty() && nonBasicTypeVersionOptional) {
                            std::string nonBasicKey =
                                nonBasicTypeName + ":" + nonBasicTypeVersionOptional.get();
                            std::map<std::string, Result> nonBasicMembers;
                            std::vector<Result> nonBasicMembersVector;
                            size_t nonBasicOffset = 0;
                            size_t nonBasicSize = 0;
                            resolveModelMembers(nonBasicKey, allNodes, structNodes, nonBasicMembers,
                                                nonBasicMembersVector, nonBasicSize,
                                                nonBasicOffset);
                            nonBasicSize = (nonBasicOffset + 3) / 4 * 4; // Pad nonBasic size

                            std::function<void(std::vector<int>)> generateArrayElements;
                            generateArrayElements = [&](std::vector<int> indices) {
                                if (indices.size() == dimensions.size()) {
                                    std::string prefix = memberName;
                                    for (int idx : indices) {
                                        prefix += "[" + std::to_string(idx) + "]";
                                    }
                                    size_t elementOffset = offset;
                                    for (const auto &res : nonBasicMembersVector) {
                                        std::string subKey = prefix + "." + res.name;
                                        Result subResult = res;
                                        subResult.offset += elementOffset;
                                        subResult.name = subKey;

                                        currentModelMembers[subKey] = subResult;
                                        currentModelMembersVector.push_back(
                                            subResult); 
                                    }
                                    offset += nonBasicSize;
                                    return;
                                }
                                int dim = dimensions[indices.size()];
                                for (int i = 0; i < dim; ++i) {
                                    auto next = indices;
                                    next.push_back(i);
                                    generateArrayElements(next);
                                }
                            };
                            generateArrayElements({});
                        } else {
                            LOG(error)
                                << "nonBasic array member '" << memberName << "' lacks version";
                        }
                    }
                } else if (sequenceMaxLengthOptional) {
                    if (memberType != "nonBasic") {
                        try {
                            int maxLength = std::stoi(sequenceMaxLengthOptional.get());
                            if (maxLength > 0) {
                                offset = (offset + 3) / 4 * 4; // Align sequence start
                                generateSequenceKeys(memberName, memberType, maxLength,
                                                     currentModelMembers, currentModelMembersVector,
                                                     offset);
                            } else {
                                LOG(error) << "Invalid sequenceMaxLength: " << maxLength;
                            }
                        } catch (const std::exception &e) {
                            LOG(error)
                                << "Invalid sequenceMaxLength: " << sequenceMaxLengthOptional.get();
                        }
                    } else {
                        LOG(error)
                            << "sequenceMaxLength not supported for nonBasic type: " << memberName;
                    }
                } else if (memberType == "nonBasic") {
                    std::string nonBasicTypeName =
                        memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName");
                    auto nonBasicTypeVersionOptional =
                        memberNode.second.get_optional<std::string>("<xmlattr>.version");
                    if (!nonBasicTypeName.empty() && nonBasicTypeVersionOptional) {
                        size_t startingOffset = (offset + 3) / 4 * 4;
                        std::map<std::string, Result> subMembers;
                        std::vector<Result> subMembersVector;
                        size_t subOffset = 0;
                        size_t subSize = 0;
                        resolveModelMembers(
                            nonBasicTypeName + ":" + nonBasicTypeVersionOptional.get(), allNodes,
                            structNodes, subMembers, subMembersVector, subSize, subOffset);
                        for (const auto &res : subMembersVector) {
                            Result newRes = res;
                            newRes.offset += startingOffset;
                            newRes.name = memberName + "." + res.name;
                            currentModelMembers[newRes.name] = newRes;
                            currentModelMembersVector.push_back(newRes);
                        }
                        offset = startingOffset + ((subOffset + 3) / 4 * 4);
                    } else {
                        LOG(error) << "nonBasic member '" << memberName << "' lacks version";
                    }
                } else {
                    size_t typeSize = getBasicTypeSize(memberType);
                    offset = (offset + 3) / 4 * 4; // Align to 4 bytes
                    currentModelMembers[memberName] =
                        Result{memberName, memberType, typeSize, (unsigned int)offset};
                    currentModelMembersVector.push_back(currentModelMembers[memberName]);
                    offset += typeSize;
                }
            }
        }
        modelSize =
            offset; // Update modelSize to final offset before padding total size in parseSchema
        visiting.erase(currentModelNameAndVersion);
    }

    size_t ModelParser::getBasicTypeSize(const std::string &type)
    {
        auto it = basicTypeSizes.find(type);
        if (it != basicTypeSizes.end()) {
            return it->second;
        }
        LOG(warning) << "Unknown basic type: " << type << ", assuming size 0";
        return 0;
    }

    void ModelParser::generateArrayKeys(const std::string &memberName,
                                        const std::string &memberType,
                                        const std::vector<int> &dimensions,
                                        std::vector<int> currentIndices,
                                        std::map<std::string, Result> &currentModelMembers,
                                        std::vector<Result> &currentModelMembersVector,
                                        size_t &offset)
    {
        if (currentIndices.size() == dimensions.size()) {
            std::string key = memberName;
            for (int index : currentIndices) {
                key += "[" + std::to_string(index) + "]";
            }
            size_t typeSize = getBasicTypeSize(memberType);
            currentModelMembers[key] = Result{key, memberType, typeSize, (unsigned int)offset};
            currentModelMembersVector.push_back(currentModelMembers[key]);
            offset += typeSize;
            return;
        }

        int currentDimension = dimensions[currentIndices.size()];
        for (int i = 0; i < currentDimension; ++i) {
            std::vector<int> nextIndices = currentIndices;
            nextIndices.push_back(i);
            generateArrayKeys(memberName, memberType, dimensions, nextIndices, currentModelMembers,
                              currentModelMembersVector, offset);
        }
    }

    void ModelParser::generateSequenceKeys(const std::string &memberName,
                                           const std::string &memberType, int maxLength,
                                           std::map<std::string, Result> &currentModelMembers,
                                           std::vector<Result> &currentModelMembersVector,
                                           size_t &offset)
    {
        size_t typeSize = getBasicTypeSize(memberType);
        for (int i = 0; i < maxLength; ++i) {
            std::string key = memberName + "[" + std::to_string(i) + "]";
            currentModelMembers[key] = Result{key, memberType, typeSize, (unsigned int)offset};
            currentModelMembersVector.push_back(currentModelMembers[key]);
            offset += typeSize;
        }
    }

} // namespace ngvs
} // namespace dsf
