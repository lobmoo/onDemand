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

    bool forwardToBuffer(const std::string &type, const std::string &value,
                         std::vector<uint8_t> &buffer)
    {

        auto it = basicTypeSizes.find(type);
        if (it == basicTypeSizes.end()) {
            throw std::invalid_argument("未知的数据类型: " + type);
        }
        buffer.clear();
        buffer.resize(it->second); // 使用已验证的 it->second

        try {
            if (type == "DT_BOOLEAN") {
                uint8_t val = (value == "1" || value == "TRUE" || value == "true") ? 1 : 0;
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_BYTE" || type == "DT_USINT") {
                uint8_t val = static_cast<uint8_t>(std::stoul(value));
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_SINT") {
                int8_t val = static_cast<int8_t>(std::stoi(value));
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_WORD" || type == "DT_UINT") {
                uint16_t val = static_cast<uint16_t>(std::stoul(value));
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_INT") {
                int16_t val = static_cast<int16_t>(std::stoi(value));
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_DWORD" || type == "DT_UDINT") {
                uint32_t val = static_cast<uint32_t>(std::stoul(value));
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_DINT") {
                int32_t val = std::stoi(value);
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_LWORD" || type == "DT_ULINT") {
                uint64_t val = std::stoull(value);
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_LINT") {
                int64_t val = std::stoll(value);
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_REAL") {
                float val = std::stof(value);
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_LREAL") {
                double val = std::stod(value);
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_CHAR") {
                if (value.length() != 1) {
                    throw std::invalid_argument("DT_CHAR需要单个字符");
                }
                uint8_t val = static_cast<uint8_t>(value[0]);
                std::memcpy(buffer.data(), &val, it->second);
            } else if (type == "DT_CHARSEQ" || type == "DT_STRING") {
            } else if (type == "DT_WCHAR") {
            } else if (type == "DT_WCHARSEQ" || type == "DT_WSTRING") { // 剩余字节由resize初始化为0
            } else {
                throw std::invalid_argument("不支持的数据类型: " + type);
            }

            return true;
        } catch (const std::exception &e) {
            throw std::runtime_error("转换失败: " + std::string(e.what()));
        }
    }

    std::string forwardToString(const std::vector<uint8_t> &buffer, const std::string &type)
    {
        auto checkSize = [&](size_t expected) {
            if (buffer.size() < expected) {
                throw std::out_of_range("Buffer too small for type: " + type);
            }
        };

        const uint8_t *data = buffer.data();

        if (type == "DT_BOOLEAN") {
            checkSize(1);
            return buffer[0] ? "true" : "false";
        } else if (type == "DT_BYTE" || type == "DT_USINT") {
            checkSize(1);
            return std::to_string(data[0]);
        } else if (type == "DT_SINT") {
            checkSize(1);
            int8_t val;
            std::memcpy(&val, data, 1);
            return std::to_string(val);
        } else if (type == "DT_WORD" || type == "DT_UINT") {
            checkSize(2);
            uint16_t val;
            std::memcpy(&val, data, 2);
            return std::to_string(val);
        } else if (type == "DT_INT") {
            checkSize(2);
            int16_t val;
            std::memcpy(&val, data, 2);
            return std::to_string(val);
        } else if (type == "DT_DWORD" || type == "DT_UDINT") {
            checkSize(4);
            uint32_t val;
            std::memcpy(&val, data, 4);
            return std::to_string(val);
        } else if (type == "DT_DINT") {
            checkSize(4);
            int32_t val;
            std::memcpy(&val, data, 4);
            return std::to_string(val);
        } else if (type == "DT_LWORD" || type == "DT_ULINT") {
            checkSize(8);
            uint64_t val;
            std::memcpy(&val, data, 8);
            return std::to_string(val);
        } else if (type == "DT_LINT") {
            checkSize(8);
            int64_t val;
            std::memcpy(&val, data, 8);
            return std::to_string(val);
        } else if (type == "DT_REAL") {
            checkSize(4);
            float val;
            std::memcpy(&val, data, 4);
            return std::to_string(val);
        } else if (type == "DT_LREAL") {
            checkSize(8);
            double val;
            std::memcpy(&val, data, 8);
            return std::to_string(val);
        } else if (type == "DT_CHAR") {
            checkSize(1);
            return std::string(1, static_cast<char>(data[0]));
        } else if (type == "DT_STRING" || type == "DT_CHARSEQ") {
            checkSize(82);
            uint16_t strlen_bytes;
            std::memcpy(&strlen_bytes, data, 2);
            if (strlen_bytes > 80)
                strlen_bytes = 80;
            return std::string(reinterpret_cast<const char *>(data + 2), strlen_bytes);
        } else if (type == "DT_WCHAR") {
            checkSize(2);
            wchar_t wchar;
            std::memcpy(&wchar, data, 2);
            return std::string(1, static_cast<char>(wchar)); // 简化处理
        } else if (type == "DT_WSTRING" || type == "DT_WCHARSEQ") {
            checkSize(508);
            uint16_t wlen;
            std::memcpy(&wlen, data, 2);
            if (wlen > 254)
                wlen = 254;
            std::wstring wstr(reinterpret_cast<const wchar_t *>(data + 2), wlen);
            std::string result;
            for (wchar_t wc : wstr)
                result += static_cast<char>(wc); // 简化
            return result;
        }

        throw std::invalid_argument("Unsupported type: " + type);
    }
    ModelParser::ModelParser(size_t alignment) : ALIGNMENT_(alignment)
    {
    }
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

    error_code_t
    ModelParser::parseSchema(std::unordered_map<std::string, ModelDefine> &modelDefines,
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
        try {
            for (const auto &modelNode : ptInput.get_child("models")) {
                if (modelNode.first == "struct") {
                    ModelDefine model;
                    model.modelName = modelNode.second.get<std::string>("<xmlattr>.name");
                    model.modelVersion = modelNode.second.get<std::string>("<xmlattr>.version");
                    model.schema = child2xml(modelNode.second, "struct");
                    model.size = 0;
                    if (modelDefines.find(model.modelName + ":" + model.modelVersion)
                        == modelDefines.end()) {
                        doParseModels.push_back(model.modelName + ":" + model.modelVersion);
                        modelDefines[model.modelName + ":" + model.modelVersion] = model;
                        structNodes_[model.modelName + ":" + model.modelVersion] = modelNode.second;    
                    } else {
                        LOG(warning) << "Model already exists: " << model.modelName << ":"
                                     << model.modelVersion;
                    }
                }
            }

        } catch (const std::exception &e) {
            LOG(error) << "Error parsing XML schema: " << e.what();
        }
        for (auto &it : doParseModels) {
            LOG(info) << "Model to parse: " << it;
            const std::string &modelNameAndVersion = it;
            auto &modelDefine = modelDefines[it];
            size_t modelSize = 0;
            size_t offset = 0;
            resolveModelMembers(modelNameAndVersion, modelDefines, modelDefine.members, modelSize,
                                offset, modelDefine.modelVersion);
            modelDefine.size = (offset + ALIGNMENT_ - 1) / ALIGNMENT_ * ALIGNMENT_;
        }
        return MODEL_PARSER_OK;
    }

    void
    ModelParser::resolveModelMembers(const std::string &currentModelNameAndVersion,
                                     std::unordered_map<std::string, ModelDefine> &allNodes,
                                     std::vector<std::shared_ptr<TreeNode>> &currentModelMembers,
                                     size_t &modelSize, size_t &offset,
                                     const std::string &modelVersion, const std::string &parentName)
    {
        if (ALIGNMENT_ <= 0) {
            LOG(error) << "Alignment must be greater than 0, current value: " << ALIGNMENT_;
            return;
        }

        if (visiting.count(currentModelNameAndVersion)) {
            LOG(error) << "Detected cyclic dependency at model: " << currentModelNameAndVersion;
            return;
        }
        visiting.insert(currentModelNameAndVersion);

        const auto &itStructNode = structNodes_.find(currentModelNameAndVersion);
        if (itStructNode == structNodes_.end()) {
            LOG(warning) << "Failed to find struct node for model: " << currentModelNameAndVersion;
            visiting.erase(currentModelNameAndVersion);
            return;
        }
        const auto &structNode = itStructNode->second;

        // 处理基类型
        auto baseTypeNameOptional = structNode.get_optional<std::string>("<xmlattr>.baseType");
        auto baseTypeVersionOptional =
            structNode.get_optional<std::string>("<xmlattr>.baseTypeVersion");
        if (baseTypeNameOptional && baseTypeVersionOptional) {
            std::string baseTypeKey =
                baseTypeNameOptional.get() + ":" + baseTypeVersionOptional.get();
            resolveModelMembers(baseTypeKey, allNodes, currentModelMembers, modelSize, offset,
                                baseTypeVersionOptional.get(), parentName);
        } else if (baseTypeNameOptional) {
            LOG(warning) << "baseType '" << baseTypeNameOptional.get()
                         << "' has no version specified in model: " << currentModelNameAndVersion;
        }

        try {
            // 处理成员
            for (const auto &memberNode : structNode.get_child("")) {
                if (memberNode.first == "member") {
                    std::string memberName = memberNode.second.get<std::string>("<xmlattr>.name");
                    std::string memberType = memberNode.second.get<std::string>("<xmlattr>.type");
                    auto arrayDimensionsOptional =
                        memberNode.second.get_optional<std::string>("<xmlattr>.arrayDimensions");
                    auto sequenceMaxLengthOptional =
                        memberNode.second.get_optional<std::string>("<xmlattr>.sequenceMaxLength");

                    // 构造成员的完整路径
                    std::string nodeName =
                        parentName.empty() ? memberName : parentName + "." + memberName;
                    std::string nodeNonBasicTypeName;
                    std::string nodeVersion;
                    if (memberType == "nonBasic") {
                        auto nonBasicTypeVersionOptional =
                            memberNode.second.get_optional<std::string>("<xmlattr>.version");
                        if (nonBasicTypeVersionOptional) {
                            nodeVersion = nonBasicTypeVersionOptional.get();
                        } else {
                            LOG(warning) << "Non-basic member '" << memberName
                                         << "' lacks version, using name without version";
                        }
                        nodeNonBasicTypeName =
                            memberNode.second.get<std::string>("<xmlattr>.nonBasicTypeName", "");
                    }

                    // 对齐成员起始偏移
                    offset = (offset + ALIGNMENT_ - 1) / ALIGNMENT_ * ALIGNMENT_;
                    size_t startingOffset = offset; // 记录起始偏移用于大小计算

                    if (arrayDimensionsOptional) {
                        // 处理数组（基本或非基本类型）
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

                        auto arrayNode = std::make_shared<TreeNode>();
                        arrayNode->name = nodeName;
                        arrayNode->type = "array";
                        arrayNode->offset = startingOffset;
                        arrayNode->nonBasicTypeName = nodeNonBasicTypeName;
                        arrayNode->version = nodeVersion;

                        std::vector<std::shared_ptr<TreeNode>> arrayElements;
                        size_t currentOffset = offset;
                        if (memberType == "nonBasic") {
                            // 非基本类型数组
                            if (!nodeNonBasicTypeName.empty() && !nodeVersion.empty()) {
                                std::string nonBasicKey = nodeNonBasicTypeName + ":" + nodeVersion;

                                std::function<void(std::vector<int>, size_t &)>
                                    generateArrayElements;
                                generateArrayElements = [&](std::vector<int> indices,
                                                            size_t &currentOffset) {
                                    if (indices.size() == dimensions.size()) {
                                        std::string elementName = nodeName;
                                        for (int idx : indices) {
                                            elementName += "[" + std::to_string(idx) + "]";
                                        }
                                        std::vector<std::shared_ptr<TreeNode>> elementMembers;
                                        size_t elementSize = 0;
                                        size_t elementOffset = 0; // 使用相对偏移量解析元素
                                        resolveModelMembers(nonBasicKey, allNodes, elementMembers,
                                                            elementSize, elementOffset, nodeVersion,
                                                            elementName);
                                        // 对齐元素大小
                                        size_t singleElementSize = (elementSize + ALIGNMENT_ - 1)
                                                                   / ALIGNMENT_ * ALIGNMENT_;

                                        auto elementNode = std::make_shared<TreeNode>();
                                        elementNode->name = elementName;
                                        elementNode->type = memberType;
                                        elementNode->size = singleElementSize;
                                        elementNode->offset = currentOffset;
                                        elementNode->is_array = true;
                                        elementNode->array_indices = indices;
                                        elementNode->nonBasicTypeName = nodeNonBasicTypeName;
                                        elementNode->version = nodeVersion;

                                        // 深拷贝 elementMembers 并调整偏移量
                                        elementNode->children = elementMembers;
                                        for (auto &child : elementNode->children) {
                                            child->offset += currentOffset; // 添加全局偏移
                                            std::function<void(
                                                std::vector<std::shared_ptr<dsf::parser::TreeNode>>
                                                    &)>
                                                adjustNestedOffsets =
                                                    [&](std::vector<std::shared_ptr<
                                                            dsf::parser::TreeNode>> &children) {
                                                        for (auto &nestedChild : children) {
                                                            nestedChild->offset += currentOffset;
                                                            adjustNestedOffsets(
                                                                nestedChild->children);
                                                        }
                                                    };
                                            adjustNestedOffsets(child->children);
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
                                generateArrayElements({}, currentOffset);
                                arrayNode->size = (currentOffset - startingOffset + ALIGNMENT_ - 1)
                                                  / ALIGNMENT_ * ALIGNMENT_;
                                offset = currentOffset;
                            } else {
                                LOG(error)
                                    << "nonBasic array member '" << memberName << "' lacks version";
                                continue;
                            }
                        } else {
                            // 基本类型数组
                            size_t typeSize = getBasicTypeSize(memberType);
                            std::function<void(std::vector<int>, size_t &)> generateArrayElements;
                            generateArrayElements = [&](std::vector<int> indices,
                                                        size_t &currentOffset) {
                                if (indices.size() == dimensions.size()) {
                                    std::string elementName = nodeName;
                                    for (int idx : indices) {
                                        elementName += "[" + std::to_string(idx) + "]";
                                    }
                                    std::shared_ptr<TreeNode> elementNode =
                                        std::make_shared<TreeNode>();
                                    elementNode->name = elementName;
                                    elementNode->type = memberType;
                                    elementNode->size = typeSize;
                                    elementNode->offset = currentOffset;
                                    elementNode->is_array = true;
                                    elementNode->array_indices = indices;
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
                            generateArrayElements({}, currentOffset);
                            arrayNode->size = (currentOffset - startingOffset + ALIGNMENT_ - 1)
                                              / ALIGNMENT_ * ALIGNMENT_;
                            offset = currentOffset;
                        }

                        arrayNode->children = std::move(arrayElements);
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

                        auto seqNode = std::make_shared<TreeNode>();
                        seqNode->name = nodeName;
                        seqNode->type = "sequence";
                        seqNode->offset = startingOffset;
                        seqNode->nonBasicTypeName = nodeNonBasicTypeName;
                        seqNode->version = nodeVersion;

                        std::vector<std::shared_ptr<TreeNode>> seqElements;
                        size_t currentOffset = offset;
                        if (memberType == "nonBasic") {
                            // 非基本类型序列
                            if (!nodeNonBasicTypeName.empty() && !nodeVersion.empty()) {
                                std::string nonBasicKey = nodeNonBasicTypeName + ":" + nodeVersion;
                                for (int i = 0; i < maxLength; ++i) {
                                    std::string elementName =
                                        nodeName + "[" + std::to_string(i) + "]";
                                    std::vector<std::shared_ptr<TreeNode>> elementMembers;
                                    size_t elementSize = 0;
                                    size_t elementOffset = 0; // 使用相对偏移解析元素
                                    resolveModelMembers(nonBasicKey, allNodes, elementMembers,
                                                        elementSize, elementOffset, nodeVersion,
                                                        elementName);
                                    // 对齐单个元素大小
                                    size_t singleElementSize =
                                        (elementSize + ALIGNMENT_ - 1) / ALIGNMENT_ * ALIGNMENT_;

                                    auto elementNode = std::make_shared<TreeNode>();
                                    elementNode->name = elementName;
                                    elementNode->type = memberType;
                                    elementNode->size = singleElementSize;
                                    elementNode->offset = currentOffset;
                                    elementNode->is_array = true;
                                    elementNode->array_indices = {i};
                                    elementNode->nonBasicTypeName = nodeNonBasicTypeName;
                                    elementNode->version = nodeVersion;
                                    // 深拷贝 elementMembers 并调整偏移
                                    elementNode->children = elementMembers;
                                    for (auto &child : elementNode->children) {
                                        child->offset += currentOffset; // 添加全局偏移
                                        std::function<void(TreeNode &)> adjustNestedOffsets =
                                            [&](TreeNode &node) {
                                                for (auto &nestedChild : node.children) {
                                                    nestedChild->offset += currentOffset;
                                                    adjustNestedOffsets(*nestedChild);
                                                }
                                            };
                                        adjustNestedOffsets(*child);
                                    }
                                    seqElements.push_back(elementNode);
                                    currentOffset += singleElementSize;
                                }
                                seqNode->size = (currentOffset - startingOffset + ALIGNMENT_ - 1)
                                                / ALIGNMENT_ * ALIGNMENT_;
                                offset = currentOffset;
                            } else {
                                LOG(error) << "nonBasic sequence member '" << memberName
                                           << "' lacks version";
                                continue;
                            }
                        } else {
                            // 基本类型序列
                            size_t typeSize = getBasicTypeSize(memberType);
                            for (int i = 0; i < maxLength; ++i) {
                                std::string elementName = nodeName + "[" + std::to_string(i) + "]";
                                auto elementNode = std::make_shared<TreeNode>();
                                elementNode->name = elementName;
                                elementNode->type = memberType;
                                elementNode->size = typeSize;
                                elementNode->offset = currentOffset;
                                elementNode->is_array = true;
                                elementNode->array_indices = {i};
                                seqElements.push_back(elementNode);
                                currentOffset += typeSize;
                            }
                            seqNode->size = (currentOffset - startingOffset + ALIGNMENT_ - 1)
                                            / ALIGNMENT_ * ALIGNMENT_;
                            offset = currentOffset;
                        }
                        seqNode->children = std::move(seqElements);
                        currentModelMembers.push_back(std::move(seqNode));
                    } else if (memberType == "nonBasic") {
                        // 处理非基本类型
                        if (!nodeNonBasicTypeName.empty() && !nodeVersion.empty()) {
                            std::string nonBasicKey = nodeNonBasicTypeName + ":" + nodeVersion;
                            std::vector<std::shared_ptr<TreeNode>> subMembers;
                            size_t subOffset = startingOffset; // 从当前全局偏移开始
                            size_t subSize = 0;
                            resolveModelMembers(nonBasicKey, allNodes, subMembers, subSize,
                                                subOffset, nodeVersion, nodeName);
                            auto node = std::make_shared<TreeNode>();
                            node->name = nodeName;
                            node->type = memberType;
                            node->size = (subOffset - startingOffset + ALIGNMENT_ - 1) / ALIGNMENT_
                                         * ALIGNMENT_;
                            node->offset = startingOffset;
                            node->children = std::move(subMembers);
                            node->nonBasicTypeName = nodeNonBasicTypeName;
                            node->version = nodeVersion;
                            offset = startingOffset + node->size;
                            currentModelMembers.push_back(std::move(node));

                        } else {
                            LOG(error) << "nonBasic member '" << memberName << "' lacks version";
                        }
                    } else {
                        // 处理基本类型
                        size_t typeSize = getBasicTypeSize(memberType);
                        auto node = std::make_shared<TreeNode>();
                        node->name = nodeName;
                        node->type = memberType;
                        node->size = typeSize;
                        node->offset = startingOffset;
                        node->nonBasicTypeName = "";
                        node->version = "";
                        currentModelMembers.push_back(node);
                        offset = startingOffset + typeSize;
                    }
                }
            }
        } catch (const boost::property_tree::ptree_bad_path &e) {
            LOG(error) << "Error processing model members for " << currentModelNameAndVersion
                       << ": " << e.what();
        }
        modelSize = offset;
        visiting.erase(currentModelNameAndVersion);
    }

    void ModelParser::updateChildNames(TreeNode &node, const std::string &parentPrefix)
    {
        for (auto &child : node.children) {
            // 如果子节点名称已包含父节点名称的一部分，避免重复拼接
            std::string newName = child->name;
            if (newName.find(parentPrefix + ".") != 0) {
                newName = parentPrefix + "." + child->name;
            }
            child->name = newName;
            updateChildNames(*child, child->name);
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

    void ModelParser::getLeafNodes(const TreeNode &node,
                                   std::vector<std::shared_ptr<dsf::parser::TreeNode>> &leaves)
    {
        if (node.children.empty()) {
            leaves.push_back(std::make_shared<TreeNode>(node));
            return;
        }
        for (const auto &child : node.children) {
            getLeafNodes(*child, leaves);
        }
    }

    bool
    ModelParser::findNodeAndGetLeaves(const ModelDefine &model, const std::string &targetName,
                                      std::vector<std::shared_ptr<dsf::parser::TreeNode>> &leaves)
    {
        std::function<bool(const std::vector<std::shared_ptr<TreeNode>> &)> search =
            [&](const std::vector<std::shared_ptr<TreeNode>> &nodes) {
                for (const auto &node : nodes) {
                    if (node->name == targetName) {
                        getLeafNodes(*node, leaves);
                        return true;
                    }
                    if (!node->children.empty()) {
                        if (search(node->children))
                            return true;
                    }
                }
                return false;
            };
        return search(model.members);
    }

    bool ModelParser::findNodeAllLeaves(const ModelDefine &model,
                                        std::vector<std::shared_ptr<dsf::parser::TreeNode>> &leaves)
    {
        if (model.members.empty()) {
            LOG(warning) << "No members found in model: " << model.modelName << ":"
                         << model.modelVersion;
            return false;
        }

        std::function<void(const std::vector<std::shared_ptr<TreeNode>> &)> collectLeaves =
            [&](const std::vector<std::shared_ptr<TreeNode>> &nodes) {
                for (const auto &node : nodes) {
                    if (node->children.empty()) {
                        leaves.push_back(node);
                    } else {
                        collectLeaves(node->children);
                    }
                }
            };

        collectLeaves(model.members);
        return !leaves.empty();
    }

    void ModelParser::printmembersInfo(std::vector<std::shared_ptr<dsf::parser::TreeNode>> &nodes)
    {
        for (const auto &node : nodes) {
            LOG(info) << "Node Name: " << node->name << ", Type: " << node->type
                      << ", Size: " << node->size << ", Offset: " << node->offset
                      << ", NonBasicTypeName: " << node->nonBasicTypeName
                      << ", Version: " << node->version
                      << ", Is Array: " << (node->is_array ? "Yes" : "No");
            for (const auto &index : node->array_indices) {
                LOG(info) << index << " ";
            }
        }
    }

    void ModelParser::printAllLeafNodesInfo(const ModelDefine &model)
    {
        // 递归收集并打印叶子节点
        std::function<void(const std::vector<std::shared_ptr<TreeNode>> &)> collectLeaves =
            [&](const std::vector<std::shared_ptr<TreeNode>> &nodes) {
                for (const auto &node : nodes) {
                    if (node->children.empty()) {
                        // 打印叶子节点
                        LOG(info) << "Leaf node: name=" << node->name << ", type=" << node->type
                                  << ", size=" << node->size << ", offset=" << node->offset
                                  << ", nonBasicTypeName=" << node->nonBasicTypeName
                                  << ", version=" << node->version;
                    } else {
                        collectLeaves(node->children); // 递归处理子节点
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

} // namespace parser
} // namespace dsf