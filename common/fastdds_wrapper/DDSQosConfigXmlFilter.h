#ifndef DDSQOSCONFIGXMLFILTER_H_
#define DDSQOSCONFIGXMLFILTER_H_
#include "tinyxml2.h"
#include <string>
#include <vector>
#include <log/logger.h>
#include <algorithm>
#include <iostream>

using namespace tinyxml2;

class DDSQosConfigXmlFilter
{

public:
    /*debug 专用*/
    bool filterQoS(const std::string &input_file, const std::string &output_file)
    {
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError result = doc.LoadFile(input_file.c_str());
        if (result != tinyxml2::XML_SUCCESS) {
            std::cerr << "Failed to load XML file: " << input_file << std::endl;
            return false;
        }

        // 创建新的 XML 文档
        tinyxml2::XMLDocument new_doc;
        auto decl = new_doc.NewDeclaration();
        new_doc.InsertFirstChild(decl);
        auto profiles = new_doc.NewElement("profiles");
        profiles->SetAttribute("xmlns", "http://www.eprosima.com");
        new_doc.InsertEndChild(profiles);

        // 处理指定的节点（participant、data_writer、data_reader）
        processNodes(doc, new_doc, profiles);

        // 保存过滤后的 XML 文件
        result = new_doc.SaveFile(output_file.c_str());
        if (result != tinyxml2::XML_SUCCESS) {
            std::cerr << "Failed to save filtered XML file: " << output_file << std::endl;
            return false;
        }

        return true;
    }

    std::string filterQoS(const std::string &input_file)
    {
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError result = doc.LoadFile(input_file.c_str());
        if (result != tinyxml2::XML_SUCCESS) {
            std::cerr << "Failed to load XML file: " << input_file << std::endl;
            return "";
        }
        // 创建新的 XML 文档
        tinyxml2::XMLDocument new_doc;
        auto decl = new_doc.NewDeclaration();
        new_doc.InsertFirstChild(decl);
        auto profiles = new_doc.NewElement("profiles");
        profiles->SetAttribute("xmlns", "http://www.eprosima.com");
        new_doc.InsertEndChild(profiles);

        // 处理指定的节点
        processNodes(doc, new_doc, profiles);

        tinyxml2::XMLPrinter printer(0, true);
        new_doc.Print(&printer);
        return printer.CStr();
    }

private:
    void processNodes(tinyxml2::XMLDocument &input_doc, tinyxml2::XMLDocument &output_doc,
                      tinyxml2::XMLElement *profiles)
    {
        tinyxml2::XMLElement *root = input_doc.FirstChildElement("profiles");
        if (!root)
            return;

        for (tinyxml2::XMLElement *elem = root->FirstChildElement(); elem;
             elem = elem->NextSiblingElement()) {
            const std::string elem_name = elem->Name();
            if (elem_name == "participant") {
                // 完整复制 participant 节点
                tinyxml2::XMLNode *cloned = elem->DeepClone(&output_doc);
                profiles->InsertEndChild(cloned);
            } else if (elem_name == "data_writer" || elem_name == "data_reader") {
                // 处理 data_writer 和 data_reader，仅保留 qos
                auto new_elem = output_doc.NewElement(elem_name.c_str());
                // 复制属性
                for (const tinyxml2::XMLAttribute *attr = elem->FirstAttribute(); attr;
                     attr = attr->Next()) {
                    new_elem->SetAttribute(attr->Name(), attr->Value());
                }
                for (tinyxml2::XMLElement *child = elem->FirstChildElement(); child;
                     child = child->NextSiblingElement()) {
                    std::string child_name = child->Name() ? child->Name() : "";
                    if (child_name == "qos") {
                        auto new_qos = output_doc.NewElement("qos");
                        filterQoSNode(child, new_qos, output_doc);
                        if (new_qos->FirstChildElement()) {
                            new_elem->InsertEndChild(new_qos);
                        }
                    } else if (child_name == "topic") {
                        tinyxml2::XMLNode *cloned = child->DeepClone(&output_doc);
                        new_elem->InsertEndChild(cloned);
                    } else if (!child_name.empty()) {
                        // 记录删除的其他子节点
                        std::cerr << "Warning: Deleted non-QoS/non-topic node under " << elem_name
                                  << ": " << child_name << std::endl;
                    }
                }
                profiles->InsertEndChild(new_elem);
            } else {
                std::cerr << "Warning: Deleted top-level node: " << elem_name << std::endl;
            }
        }
    }

    // 过滤单个 qos 节点，仅保留白名单中的属性
    void filterQoSNode(tinyxml2::XMLElement *input_qos, tinyxml2::XMLElement *output_qos,
                       tinyxml2::XMLDocument &output_doc)
    {
        for (tinyxml2::XMLElement *qos_child = input_qos->FirstChildElement(); qos_child;
             qos_child = qos_child->NextSiblingElement()) {
            const char *name = qos_child->Name();
            std::string qos_name = name ? name : "";
            if (!qos_name.empty()
                && std::find(allowed_qos.begin(), allowed_qos.end(), qos_name)
                       != allowed_qos.end()) {
                tinyxml2::XMLNode *cloned = qos_child->DeepClone(&output_doc);
                output_qos->InsertEndChild(cloned);
            } else if (!qos_name.empty()) {
                std::cerr << "Warning: Deleted invalid QoS attribute: " << qos_name << std::endl;
            }
        }
    }

private:
    /*<data_sharing>
        <deadline>
        <disable_heartbeat_piggyback>
        <disablePositiveAcks>
        <durability>
        <entity_factory>
        <groupData>
        <latencyBudget>
        <lifespan>
        <liveliness>
        <ownership>
        <ownershipStrength>
        <partition>
        <publishMode>
        <reliability>
        <topicData>
        <userData>*/
    const std::vector<std::string> allowed_qos = {"durability", "reliability", "history"};
};

#endif // DDSQOSCONFIGXMLFILTER_H_
