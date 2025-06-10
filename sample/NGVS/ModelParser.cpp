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
namespace parser
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
        /*解决父子版本继承的结构*/
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

                offset = (offset + 3) / 4 * 4;  // Align start of member
                size_t startingOffset = offset; // Record start for size calculation
                if (arrayDimensionsOptional) {
                    // Handle array (basic or nonBasic)
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

                    TreeNode arrayNode;
                    arrayNode.name = (memberType == "nonBasic") ? nodeName : memberName;
                    arrayNode.type = "array";
                    arrayNode.offset = startingOffset;
                    arrayNode.nonBasicTypeName = nodeNonBasicTypeName;
                    arrayNode.version = nodeVersion;

                    std::vector<TreeNode> arrayElements;
                    if (memberType == "nonBasic") {
                        // NonBasic array
                        if (!nodeNonBasicTypeName.empty() && !nodeVersion.empty()) {
                            std::string nonBasicKey = nodeNonBasicTypeName + ":" + nodeVersion;
                            std::vector<TreeNode> elementMembers;
                            size_t elementSize = 0;
                            size_t elementOffset = 0; // Use relative offset for element parsing
                            resolveModelMembers(nonBasicKey, allNodes, structNodes, elementMembers,
                                                elementSize, elementOffset, nodeVersion);
                            size_t singleElementSize =
                                (elementSize + 3) / 4 * 4; // Align element size

                            std::function<void(std::vector<int>, size_t &)> generateArrayElements;
                            generateArrayElements = [&](std::vector<int> indices,
                                                        size_t &currentOffset) {
                                if (indices.size() == dimensions.size()) {
                                    std::string elementName = memberName;
                                    for (int idx : indices) {
                                        elementName += "[" + std::to_string(idx) + "]";
                                    }
                                    if (memberType == "nonBasic") {
                                        elementName += ":" + nodeVersion;
                                    }
                                    TreeNode elementNode;
                                    elementNode.name = elementName;
                                    elementNode.type = memberType;
                                    elementNode.size = singleElementSize;
                                    elementNode.offset = currentOffset;
                                    elementNode.is_array = true;
                                    elementNode.array_indices = indices;
                                    elementNode.nonBasicTypeName = nodeNonBasicTypeName;
                                    elementNode.version = nodeVersion;

                                    // Deep copy elementMembers and adjust offsets globally
                                    elementNode.children = elementMembers;
                                    for (auto &child : elementNode.children) {
                                        child.offset =
                                            currentOffset + child.offset; // Add global offset
                                        // Recursively adjust offsets for nested children
                                        std::function<void(TreeNode &)> adjustNestedOffsets =
                                            [&](TreeNode &node) {
                                                for (auto &nestedChild : node.children) {
                                                    nestedChild.offset =
                                                        currentOffset + nestedChild.offset;
                                                    adjustNestedOffsets(nestedChild);
                                                }
                                            };
                                        adjustNestedOffsets(child);
                                    }
                                    arrayElements.push_back(elementNode);
                                    currentOffset += singleElementSize;
                                    return;
                                }
                                int dim = dimensions[indices.size()];
                                for (int i = 0; i < dim; ++i) {
                                    auto next = indices;
                                    next.push_back(i);
                                    generateArrayElements(next, currentOffset);
                                }
                            };
                            generateArrayElements({}, offset);
                            arrayNode.size = (offset - startingOffset + 3) / 4 * 4;
                        } else {
                            LOG(error)
                                << "nonBasic array member '" << memberName << "' lacks version";
                            continue;
                        }
                    } else {
                        // Basic type array
                        size_t typeSize = getBasicTypeSize(memberType);
                        std::function<void(std::vector<int>, size_t &)> generateArrayElements;
                        generateArrayElements = [&](std::vector<int> indices,
                                                    size_t &currentOffset) {
                            if (indices.size() == dimensions.size()) {
                                std::string elementName = memberName;
                                for (int idx : indices) {
                                    elementName += "[" + std::to_string(idx) + "]";
                                }
                                TreeNode elementNode;
                                elementNode.name = elementName;
                                elementNode.type = memberType;
                                elementNode.size = typeSize;
                                elementNode.offset = currentOffset;
                                elementNode.is_array = true;
                                elementNode.array_indices = indices;
                                arrayElements.push_back(elementNode);
                                currentOffset += typeSize;
                                return;
                            }
                            int dim = dimensions[indices.size()];
                            for (int i = 0; i < dim; ++i) {
                                auto next = indices;
                                next.push_back(i);
                                generateArrayElements(next, currentOffset);
                            }
                        };
                        /*这里递归调用出多维数组*/
                        generateArrayElements({}, offset);
                        arrayNode.size = (offset - startingOffset + 3) / 4 * 4;
                    }

                    arrayNode.children = std::move(arrayElements);
                    currentModelMembers.push_back(std::move(arrayNode));
                } else if (sequenceMaxLengthOptional) {
                    int maxLength;
                    try {
                        maxLength = std::stoi(sequenceMaxLengthOptional.get());
                        if (maxLength <= 0) {
                            LOG(error) << "Invalid sequenceMaxLength: " << maxLength;
                            continue;
                        }
                    } catch (const std::exception &e) {
                        LOG(error)
                            << "Invalid sequenceMaxLength: " << sequenceMaxLengthOptional.get();
                        continue;
                    }

                    TreeNode seqNode;
                    seqNode.name = (memberType == "nonBasic") ? nodeName : memberName;
                    seqNode.type = "sequence";
                    seqNode.offset = startingOffset;
                    seqNode.nonBasicTypeName = nodeNonBasicTypeName;
                    seqNode.version = nodeVersion;

                    std::vector<TreeNode> seqElements;
                    if (memberType == "nonBasic") {
                        // NonBasic sequence
                        if (!nodeNonBasicTypeName.empty() && !nodeVersion.empty()) {
                            std::string nonBasicKey = nodeNonBasicTypeName + ":" + nodeVersion;
                            std::vector<TreeNode> elementMembers;
                            size_t elementSize = 0;
                            size_t elementOffset = 0; // Use relative offset for element parsing
                            resolveModelMembers(nonBasicKey, allNodes, structNodes, elementMembers,
                                                elementSize, elementOffset, nodeVersion);
                            size_t singleElementSize =
                                (elementSize + 3) / 4 * 4; // Align element size

                            for (int i = 0; i < maxLength; ++i) {
                                std::string elementName =
                                    memberName + "[" + std::to_string(i) + "]";
                                if (memberType == "nonBasic") {
                                    elementName += ":" + nodeVersion;
                                }
                                TreeNode elementNode;
                                elementNode.name = elementName;
                                elementNode.type = memberType;
                                elementNode.size = singleElementSize;
                                elementNode.offset = offset;
                                elementNode.is_array = true;
                                elementNode.array_indices = {i};
                                elementNode.nonBasicTypeName = nodeNonBasicTypeName;
                                elementNode.version = nodeVersion;
                                elementNode.children = elementMembers;
                                // Adjust child offsets to be global
                                for (auto &child : elementNode.children) {
                                    child.offset = offset + child.offset; // Add global offset
                                    // Recursively adjust nested offsets
                                    std::function<void(TreeNode &)> adjustNestedOffsets =
                                        [&](TreeNode &node) {
                                            for (auto &nestedChild : node.children) {
                                                nestedChild.offset = offset + nestedChild.offset;
                                                adjustNestedOffsets(nestedChild);
                                            }
                                        };
                                    adjustNestedOffsets(child);
                                }
                                seqElements.push_back(elementNode);
                                offset += singleElementSize;
                            }
                            seqNode.size = (offset - startingOffset + 3) / 4 * 4;
                        } else {
                            LOG(error)
                                << "nonBasic sequence member '" << memberName << "' lacks version";
                            continue;
                        }
                    } else {
                        // Basic type sequence (unchanged, as it doesn't involve nested children)
                        size_t typeSize = getBasicTypeSize(memberType);
                        for (int i = 0; i < maxLength; ++i) {
                            std::string elementName = memberName + "[" + std::to_string(i) + "]";
                            TreeNode elementNode;
                            elementNode.name = elementName;
                            elementNode.type = memberType;
                            elementNode.size = typeSize;
                            elementNode.offset = offset;
                            elementNode.is_array = true;
                            elementNode.array_indices = {i};
                            seqElements.push_back(elementNode);
                            offset += typeSize;
                        }
                        seqNode.size = (offset - startingOffset + 3) / 4 * 4;
                    }
                    seqNode.children = std::move(seqElements);
                    currentModelMembers.push_back(std::move(seqNode));
                } else if (memberType == "nonBasic") {
                    // Handle nonBasic type
                    if (!nodeNonBasicTypeName.empty() && !nodeVersion.empty()) {
                        std::string nonBasicKey = nodeNonBasicTypeName + ":" + nodeVersion;
                        std::vector<TreeNode> subMembers;
                        size_t subOffset = startingOffset; // Start at current global offset
                        size_t subSize = 0;
                        resolveModelMembers(nonBasicKey, allNodes, structNodes, subMembers, subSize,
                                            subOffset, nodeVersion);
                        TreeNode node;
                        node.name = nodeName;
                        node.type = memberType;
                        node.size = (subOffset - startingOffset + 3) / 4 * 4;
                        node.offset = startingOffset;
                        node.children = std::move(subMembers);
                        node.nonBasicTypeName = nodeNonBasicTypeName;
                        node.version = nodeVersion;
                        currentModelMembers.push_back(std::move(node));
                        offset = startingOffset + node.size;
                    } else {
                        LOG(error) << "nonBasic member '" << memberName << "' lacks version";
                    }
                } else {
                    // Handle basic type
                    size_t typeSize = getBasicTypeSize(memberType);
                    TreeNode node;
                    node.name = nodeName;
                    node.type = memberType;
                    node.size = typeSize;
                    node.offset = startingOffset;
                    node.nonBasicTypeName = "";
                    node.version = "";
                    currentModelMembers.push_back(std::move(node));
                    offset = startingOffset + typeSize;
                }
            }
        }
        modelSize = offset;
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

    bool ModelParser::findNodeAllLeaves(const ModelDefine &model, std::vector<TreeNode> &leaves)
    {
        if (model.members.empty()) {
            LOG(warning) << "No members found in model: " << model.modelName << ":"
                         << model.modelVersion;
            return false;
        }

        std::function<void(const std::vector<TreeNode> &)> collectLeaves =
            [&](const std::vector<TreeNode> &nodes) {
                for (const auto &node : nodes) {
                    if (node.children.empty()) {
                        leaves.push_back(node);
                    } else {
                        collectLeaves(node.children);
                    }
                }
            };

        collectLeaves(model.members);
        return !leaves.empty();
    }

    void ModelParser::printmembersInfo(std::vector<TreeNode> &nodes)
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

    void ModelParser::printAllLeafNodesInfo(const ModelDefine &model) const
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

    void ModelParser::reCalcuOffset(ModelDefine &model)
    {
        std::vector<TreeNode> leaves;
        findNodeAllLeaves(model, leaves);
        size_t offset = 0;
        for (auto &leaf : leaves)
        {
            leaf.offset = offset;
            offset += leaf.size;
            offset = (offset + 3) / 4 * 4; // 对齐到4字节边界
        }
        return;
    }
} // namespace parser
} // namespace dsf