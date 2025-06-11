/**
 * @file KeyValueSerialize.cpp
 * @brief 
 * @author wqb (894424179@qq.com)
 * @version 1.0
 * @date 2025-06-09
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-06-09     <td>1.0     <td>wqb   <td>初版
 * </table>
 */
#include "KeyValueSerialize.h"
#include "log/logger.h"
#include <cmath>
namespace dsf
{
namespace ngvs
{

    KeyValueSerializer::KeyValueSerializer(size_t alignment) : ALIGNMENT_(alignment)
    {
    }
    // 要注意offset、越界等问题，多写点错误判断
    bool KeyValueSerializer::serialize(const std::string &schema, const std::string &ModelName, 
                                                const std::unordered_map<std::string, char *> &data, std::vector<char> &outBuffer)
    {
        parser::ModelParser parser;
        /* 1.先查询是否已经解析过model, 如果没解析过则调用ModelParser来解析所有model的结构 */
        auto it = modelDefines_.find(ModelName);
        if(it == modelDefines_.end())
        {
            //没查询到 需要解析
            std::string error_message;
            parser::error_code_t ret = parser.parseSchema(modelDefines_, schema, error_message);
            if (ret != dsf::parser::MODEL_PARSER_OK) {
                LOG(error) << "parse schema failed, ret: " << ret;
                return false;
            }
        }
        


        /* 2.判断解析的xml中是否含有该model */
        it = modelDefines_.find(ModelName);
        if (it == modelDefines_.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }

        /* 3.找到model, 输出其结构 */
        parser::ModelDefine modelDefine = it->second;
        std::vector<parser::TreeNode> leaves;
        parser.findNodeAllLeaves(modelDefine, leaves);
        parser.printmembersInfo(leaves);

        

        /* 4. 申请一段空间(大小是固定的)并且顺序地将key-value对填入 */
        leaves.clear();
        parser.findNodeAllLeaves(modelDefine, leaves);
        outBuffer.reserve(modelDefine.size);
        for(auto &leaf : leaves)
        {
            const std::string &key = leaf.name;
            auto it = data.find(key);
            if(it == data.end())
            {
                LOG(error) << "Key not found in data: " << key;
                return false; 
            }
            auto value = it->second;
            if(!serializeKeyValuePair(leaf, key, value, modelDefine, outBuffer))// 第idx个叶子节点
            {
                LOG(error) << "Failed to serialize key-value pair: " << key;
                return false;
            }
        } 

        return true;
    }

    bool KeyValueSerializer::serializeKeyValuePair(dsf::parser::TreeNode &leaf, const std::string &key,
                                                    const char* value, const parser::ModelDefine &modelDefine, std::vector<char> &outBuffer)
    {
        LOG(info) << "Key-Value Pair Serialization, Key:" << key 
                  << ", Offset: " << leaf.offset 
                  << ", Size: " << leaf.size; 

        /* 1.根据offset和size，填充buffer_ */
        if(leaf.offset + leaf.size > modelDefine.size) {
            LOG(error) << "Buffer overflow for key: " << key;
            return false;
        }
        
        // LOG(error) << "Before memcpy:" << std::endl;
        // for (int i = 0; i < modelDefine.size; ++i)
        //     printf("%02x ", static_cast<unsigned char>(outBuffer[i]));
        // printf("\n");

        std::memcpy(outBuffer.data() + leaf.offset, value, leaf.size);

        // LOG(error) << "After memcpy:" << std::endl;
        // for (int i = 0; i < modelDefine.size; ++i)
        //     printf("%02x ", static_cast<unsigned char>(outBuffer[i]));
        // printf("\n");
        return true;
    }


    bool KeyValueSerializer::deserialize(const std::string &schema, const std::string &ModelName,
                         const std::vector<char> &inBuffer, std::unordered_map<std::string, char *> &outData)
    {
        parser::ModelParser parser;
        /* 1.先查询是否已经解析过model, 如果没解析过则调用ModelParser来解析所有model的结构 */
        auto it = modelDefines_.find(ModelName);
        if(it == modelDefines_.end())
        {
            //没查询到 需要解析
            std::string error_message;
            parser::error_code_t ret = parser.parseSchema(modelDefines_, schema, error_message);
            if (ret != dsf::parser::MODEL_PARSER_OK) {
                LOG(error) << "parse schema failed, ret: " << ret;
                return false;
            }
        }
        
        /* 2.判断解析的xml中是否含有该model */
        it = modelDefines_.find(ModelName);
        if (it == modelDefines_.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }
        auto modelDefine = it->second;

        /* 3. 从inBuffer中顺序拿取data，然后构造成key-value pair并放入outData中*/
        std::vector<parser::TreeNode> leaves;
        parser.findNodeAllLeaves(modelDefine, leaves);
        for(const auto &leaf : leaves)
        {
            std::string key = leaf.name;
            if(leaf.offset + leaf.size > modelDefine.size) {
                LOG(error) << "Buffer overflow for key: " << key;
                return false;
            }

            char *value = new char[leaf.size];
            std::memcpy(value , inBuffer.data() + leaf.offset, leaf.size);

            // 将构造好的数据放入outData中
            outData.emplace(key, value);
        }

        return true;
    }

    KeyValueSerializer::~KeyValueSerializer()
    {
    }

} // namespace ngvs
} // namespace dsf
