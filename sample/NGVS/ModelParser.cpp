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
            resolveModelMembers(modelNameAndVersion, modelDefines, structNodes, modelDefine.members,
                                modelSize, offset, modelDefine.modelVersion);
            modelDefine.size = (offset + 3) / 4 * 4;
        }

        return MODEL_PARSER_OK;
    }

    void ModelParser::resolveModelMembers(
        const std::string &currentModelNameAndVersion, std::map<std::string, ModelDefine> &allNodes,
        std::map<std::string, boost::property_tree::ptree> &structNodes,
        std::vector<TreeNode> &currentModelMembers, size_t &modelSize, size_t &offset,
        const std::string &modelVersion)
    {
        if (visiting.count(currentModelNameAndVersion)) {
            LOG(error) << "Detected cyclic dependency at model: " << currentModelNameAndVersion;
            return;
        }
        visiting.insert(currentModelNameAndVersion);
        const auto &itStructNode = structNodes.find(currentModelNameAndVersion);
        if (itStructNode == structNodes.end()) {
            LOG(warning) << "Failed to find struct node for model: " << currentModelNameAndVersion;
            visiting.erase(currentModelNameAndVersion);
            return;
        }
        const auto &structNode = itStructNode->second;

        // Handle baseType
        auto baseTypeNameOptional = structNode.get_optional<std::string>("<xmlattr>.baseType");
        auto baseTypeVersionOptional =
            structNode.get_optional<std::string>("<xmlattr>.baseTypeVersion");
        if (baseTypeNameOptional && baseTypeVersionOptional) {
            std::string baseTypeKey =
                baseTypeNameOptional.get() + ":" + baseTypeVersionOptional.get();
            resolveModelMembers(baseTypeKey, allNodes, structNodes, currentModelMembers, modelSize,
                                offset, baseTypeVersionOptional.get());
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

                std::string nodeName = memberName;
                std::string nodeNonBasicTypeName;
                std::string nodeVersion;
                if (memberType == "nonBasic") {
                    auto nonBasicTypeVersionOptional =
                        memberNode.second.get_optional<std::string>("<xmlattr>.version");
                    if (nonBasicTypeVersionOptional) {
                        nodeName = memberName + ":" + nonBasicTypeVersionOptional.get();
                        nodeVersion = nonBasicTypeVersionOptional.get();
                    } else {
                        LOG(warning) << "Non-basic member '" << memberName
                                     << "' lacks version, using name without version";
                    }
                    nodeNonBasicTypeName =
                        memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName", "");
                }

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
                        generateArrayKeys(memberName, memberType, dimensions, {}, modelVersion,
                                          currentModelMembers, offset);
                    } else {
                        std::string nonBasicTypeName =
                            memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName");
                        auto nonBasicTypeVersionOptional =
                            memberNode.second.get_optional<std::string>("<xmlattr>.version");
                        if (!nonBasicTypeName.empty() && nonBasicTypeVersionOptional) {
                            std::string nonBasicKey =
                                nonBasicTypeName + ":" + nonBasicTypeVersionOptional.get();
                            std::string nonBasicVersion = nonBasicTypeVersionOptional.get();
                            std::vector<TreeNode> nonBasicMembers;
                            size_t nonBasicOffset =
                                offset; // Start sub-offset at current global offset
                            size_t nonBasicSize = 0;
                            resolveModelMembers(nonBasicKey, allNodes, structNodes, nonBasicMembers,
                                                nonBasicSize, nonBasicOffset, nonBasicVersion);
                            nonBasicSize =
                                (nonBasicOffset - offset + 3) / 4 * 4; // Size of nonBasic type

                            std::function<void(std::vector<int>)> generateArrayElements;
                            generateArrayElements = [&](std::vector<int> indices) {
                                if (indices.size() == dimensions.size()) {
                                    std::string prefix = memberName;
                                    for (int idx : indices) {
                                        prefix += "[" + std::to_string(idx) + "]";
                                    }
                                    std::string versionedPrefix = prefix + ":" + nonBasicVersion;
                                    TreeNode arrayNode;
                                    arrayNode.name = versionedPrefix;
                                    arrayNode.type = memberType;
                                    arrayNode.size = nonBasicSize;
                                    arrayNode.offset = offset; // Global offset for array node
                                    arrayNode.is_array = true;
                                    arrayNode.array_indices = indices;
                                    arrayNode.children =
                                        nonBasicMembers; // Sub-members with global offsets
                                    arrayNode.nonBasicTypeName = nonBasicTypeName;
                                    arrayNode.version = nonBasicVersion;
                                    // Adjust child offsets to be global
                                    for (auto &child : arrayNode.children) {
                                        child.offset += offset - arrayNode.children.front().offset;
                                    }
                                    currentModelMembers.push_back(arrayNode);
                                    offset += nonBasicSize; // Update global offset
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
                                offset = (offset + 3) / 4 * 4;
                                generateSequenceKeys(memberName, memberType, maxLength,
                                                     modelVersion, currentModelMembers, offset);
                            } else {
                                LOG(error) << "Invalid sequenceMaxLength: " << maxLength;
                            }
                        } catch (const std::exception &e) {
                            LOG(error)
                                << "Invalid sequenceMaxLength: " << sequenceMaxLengthOptional.get();
                        }
                    } else {
                        std::string nonBasicTypeName =
                            memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName");
                        auto nonBasicTypeVersionOptional =
                            memberNode.second.get_optional<std::string>("<xmlattr>.version");
                        if (!nonBasicTypeName.empty() && nonBasicTypeVersionOptional) {
                            std::string nonBasicKey =
                                nonBasicTypeName + ":" + nonBasicTypeVersionOptional.get();
                            std::string nonBasicVersion = nonBasicTypeVersionOptional.get();
                            std::vector<TreeNode> nonBasicMembers;
                            size_t nonBasicOffset =
                                offset; // Start sub-offset at current global offset
                            size_t nonBasicSize = 0;
                            resolveModelMembers(nonBasicKey, allNodes, structNodes, nonBasicMembers,
                                                nonBasicSize, nonBasicOffset, nonBasicVersion);
                            nonBasicSize = (nonBasicOffset - offset + 3) / 4 * 4;

                            int maxLength;
                            try {
                                maxLength = std::stoi(sequenceMaxLengthOptional.get());
                                if (maxLength <= 0) {
                                    LOG(error) << "Invalid sequenceMaxLength: " << maxLength;
                                    continue;
                                }
                            } catch (const std::exception &e) {
                                LOG(error) << "Invalid sequenceMaxLength: "
                                           << sequenceMaxLengthOptional.get();
                                continue;
                            }

                            for (int i = 0; i < maxLength; ++i) {
                                std::string key = memberName + "[" + std::to_string(i) + "]";
                                std::string versionedKey = key + ":" + nonBasicVersion;
                                TreeNode seqNode;
                                seqNode.name = versionedKey;
                                seqNode.type = memberType;
                                seqNode.size = nonBasicSize;
                                seqNode.offset = offset; // Global offset for sequence node
                                seqNode.is_array = true;
                                seqNode.array_indices = {i};
                                seqNode.children =
                                    nonBasicMembers; // Sub-members with global offsets
                                seqNode.nonBasicTypeName = nonBasicTypeName;
                                seqNode.version = nonBasicVersion;
                                // Adjust child offsets to be global
                                for (auto &child : seqNode.children) {
                                    child.offset += offset - seqNode.children.front().offset;
                                }
                                currentModelMembers.push_back(seqNode);
                                offset += nonBasicSize; // Update global offset
                            }
                        } else {
                            LOG(error)
                                << "nonBasic sequence member '" << memberName << "' lacks version";
                        }
                    }
                } else if (memberType == "nonBasic") {
                    std::string nonBasicTypeName =
                        memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName");
                    auto nonBasicTypeVersionOptional =
                        memberNode.second.get_optional<std::string>("<xmlattr>.version");
                    if (!nonBasicTypeName.empty() && nonBasicTypeVersionOptional) {
                        std::string nonBasicKey =
                            nonBasicTypeName + ":" + nonBasicTypeVersionOptional.get();
                        std::string nonBasicVersion = nonBasicTypeVersionOptional.get();
                        size_t startingOffset = (offset + 3) / 4 * 4; // Align parent node
                        std::vector<TreeNode> subMembers;
                        size_t subOffset = startingOffset; // Start sub-offset at global offset
                        size_t subSize = 0;
                        resolveModelMembers(nonBasicKey, allNodes, structNodes, subMembers, subSize,
                                            subOffset, nonBasicVersion);
                        TreeNode node;
                        node.name = nodeName;
                        node.type = memberType;
                        node.size =
                            (subOffset - startingOffset + 3) / 4 * 4; // Size of nonBasic node
                        node.offset = startingOffset; // Global offset for parent node
                        node.children = subMembers;   // Sub-members with global offsets
                        node.nonBasicTypeName = nonBasicTypeName;
                        node.version = nonBasicVersion;
                        currentModelMembers.push_back(node);
                        offset = startingOffset + node.size; // Update global offset
                    } else {
                        LOG(error) << "nonBasic member '" << memberName << "' lacks version";
                    }
                } else {
                    size_t typeSize = getBasicTypeSize(memberType);
                    offset = (offset + 3) / 4 * 4; // Align basic type
                    TreeNode node;
                    node.name = nodeName;
                    node.type = memberType;
                    node.size = typeSize;
                    node.offset = offset; // Global offset for basic type
                    node.nonBasicTypeName = "";
                    node.version = "";
                    currentModelMembers.push_back(node);
                    offset += typeSize; // Update global offset
                }
            }
        }
        modelSize = offset;
        visiting.erase(currentModelNameAndVersion);
    }

    void ModelParser::generateArrayKeys(const std::string &memberName,
                                        const std::string &memberType,
                                        const std::vector<int> &dimensions,
                                        std::vector<int> currentIndices,
                                        const std::string &modelVersion,
                                        std::vector<TreeNode> &currentModelMembers, size_t &offset)
    {
        if (currentIndices.size() == dimensions.size()) {
            std::string key = memberName;
            for (int index : currentIndices) {
                key += "[" + std::to_string(index) + "]";
            }
            std::string nodeName = (basicTypeSizes.find(memberType) == basicTypeSizes.end())
                                       ? (key + ":" + modelVersion)
                                       : key;
            size_t typeSize = getBasicTypeSize(memberType);
            TreeNode node;
            node.name = nodeName;
            node.type = memberType;
            node.size = typeSize;
            node.offset = offset;
            node.is_array = true;
            node.array_indices = currentIndices;
            node.nonBasicTypeName =
                (basicTypeSizes.find(memberType) == basicTypeSizes.end()) ? memberType : "";
            node.version =
                (basicTypeSizes.find(memberType) == basicTypeSizes.end()) ? modelVersion : "";
            currentModelMembers.push_back(node);
            offset += typeSize;
            return;
        }

        int currentDimension = dimensions[currentIndices.size()];
        for (int i = 0; i < currentDimension; ++i) {
            std::vector<int> nextIndices = currentIndices;
            nextIndices.push_back(i);
            generateArrayKeys(memberName, memberType, dimensions, nextIndices, modelVersion,
                              currentModelMembers, offset);
        }
    }

    void ModelParser::generateSequenceKeys(const std::string &memberName,
                                           const std::string &memberType, int maxLength,
                                           const std::string &modelVersion,
                                           std::vector<TreeNode> &currentModelMembers,
                                           size_t &offset)
    {
        size_t typeSize = getBasicTypeSize(memberType);
        for (int i = 0; i < maxLength; ++i) {
            std::string key = memberName + "[" + std::to_string(i) + "]";
            std::string nodeName = (basicTypeSizes.find(memberType) == basicTypeSizes.end())
                                       ? (key + ":" + modelVersion)
                                       : key;
            TreeNode node;
            node.name = nodeName;
            node.type = memberType;
            node.size = typeSize;
            node.offset = offset;
            node.is_array = true;
            node.array_indices = {i};
            node.nonBasicTypeName =
                (basicTypeSizes.find(memberType) == basicTypeSizes.end()) ? memberType : "";
            node.version =
                (basicTypeSizes.find(memberType) == basicTypeSizes.end()) ? modelVersion : "";
            currentModelMembers.push_back(node);
            offset += typeSize;
        }
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

    void ModelParser::getLeafNodes(const TreeNode &node, std::vector<TreeNode> &leaves)
    {
        if (node.children.empty()) {
            leaves.push_back(node);
            return;
        }
        for (const auto &child : node.children) {
            getLeafNodes(child, leaves);
        }
    }

    bool ModelParser::findNodeAndGetLeaves(const ModelDefine &model, const std::string &targetName,
                                           std::vector<TreeNode> &leaves)
    {
        std::function<bool(const std::vector<TreeNode> &)> search =
            [&](const std::vector<TreeNode> &nodes) {
                for (const auto &node : nodes) {
                    if (node.name == targetName) {
                        getLeafNodes(node, leaves);
                        return true;
                    }
                    if (!node.children.empty()) {
                        if (search(node.children))
                            return true;
                    }
                }
                return false;
            };
        return search(model.members);
    }

    void ModelParser::printInfo(std::vector<TreeNode> &nodes)
    {
        for (auto node : nodes)

        {
            LOG(info) << "Node Name: " << node.name << ", Type: " << node.type
                      << ", Size: " << node.size << ", Offset: " << node.offset
                      << ", NonBasicTypeName: " << node.nonBasicTypeName
                      << ", Version: " << node.version
                      << ", Is Array: " << (node.is_array ? "Yes" : "No");
            for (const auto &index : node.array_indices) {
                LOG(info) << index << " ";
            }
        }
    }

    void ModelParser::printLeafNodes(const ModelDefine &model) const
    {
        // 递归收集并打印叶子节点
        std::function<void(const std::vector<TreeNode> &)> collectLeaves =
            [&](const std::vector<TreeNode> &nodes) {
                for (const auto &node : nodes) {
                    if (node.children.empty()) {
                        // 打印叶子节点
                        LOG(info) << "Leaf node: name=" << node.name << ", type=" << node.type
                                  << ", size=" << node.size << ", offset=" << node.offset
                                  << ", nonBasicTypeName=" << node.nonBasicTypeName
                                  << ", version=" << node.version;
                    } else {
                        collectLeaves(node.children); // 递归处理子节点
                    }
                }
            };

        if (model.members.empty()) {
            LOG(warning) << "No members found in model: " << model.modelName << ":"
                         << model.modelVersion;
            return;
        }

        LOG(info) << "Printing leaf nodes for model: " << model.modelName << ":"
                  << model.modelVersion;
        collectLeaves(model.members);
    }

} // namespace ngvs
} // namespace dsf