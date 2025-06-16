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
namespace kvpair
{

    KeyValueSerializer::KeyValueSerializer(size_t alignment) : ALIGNMENT_(alignment)
    {
    }

    bool KeyValueSerializer::init(const std::string &schema)
    {
        /* 调用ModelParser来解析所有model的结构 */
        parser::ModelParser parser;
        std::string error_message;
        parser::error_code_t ret = parser.parseSchema(modelDefines_, schema, error_message);
        if (ret != dsf::parser::MODEL_PARSER_OK) {
            LOG(error) << "parse schema failed, ret: " << ret;
            return false;
        }
    }
    bool KeyValueSerializer::serialize(const std::string &ModelName, 
                                                const std::unordered_map<std::string,std::string> &data, std::vector<char> &outBuffer)
    {

        parser::ModelParser parser;
        /* 1.判断解析的xml中是否含有该model */
        auto it = modelDefines_.find(ModelName);
        if (it == modelDefines_.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }

        /* 2.找到model, 输出其结构 */
        parser::ModelDefine modelDefine = it->second;
        std::vector<parser::TreeNode> leaves;
        parser.findNodeAllLeaves(modelDefine, leaves);
        parser.printmembersInfo(leaves);

        /* 3. 申请一段空间(大小是固定的)并且顺序地将key-value对填入 */
        parser.findNodeAllLeaves(modelDefine, leaves);
        outBuffer.clear();
        outBuffer.resize(modelDefine.size);
        std::vector<uint8_t> memberData;
        for(auto &leaf : leaves)
        {
            const std::string &key = leaf.name;
            auto it = data.find(key);
            if(it == data.end())
            {
                LOG(warning) << "Key not found in data: " << key;
                std::memset(outBuffer.data() + leaf.offset, 0, leaf.size);
                continue;
            }
            size_t memberSize = leaf.size;
            memberData.clear();
            try {
                dsf::parser::forwardToBuffer(leaf.type, it->second, memberData);  // 类型转换，转成 uint8_t
            } catch (const std::exception &e) {
                LOG(error) << "Error converting value for key: " << it->first
                           << ", Error: " << e.what();
                //todo 后续考虑是否继续
                return false;
            }
            if(leaf.offset + leaf.size > modelDefine.size)
            {
                LOG(error) << "size: " << leaf.size << ", offset: " << leaf.offset << "modelDefine_size: " << modelDefine.size;
                LOG(error) << "Buffer overflow for key: " << key;
                return false;
            }
            
            std::memcpy(outBuffer.data() + leaf.offset, memberData.data(), std::min(it->second.size(), memberSize));

        } 

        return true;
    }



    bool KeyValueSerializer::deserialize(const std::string &ModelName,
                         const std::vector<char> &inBuffer, std::unordered_map<std::string, std::string> &outData)
    {  
        /* 1.判断解析的xml中是否含有该model */
        parser::ModelParser parser;      
        auto it = modelDefines_.find(ModelName);
        if (it == modelDefines_.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }
        auto modelDefine = it->second;

        /* 2. 从inBuffer中顺序拿取data，然后构造成key-value pair并放入outData中*/
        std::vector<parser::TreeNode> leaves;
        parser.findNodeAllLeaves(modelDefine, leaves);
        for(const auto &leaf : leaves)
        {
            std::string key = leaf.name;
            if(leaf.offset + leaf.size > modelDefine.size) {
                LOG(error) << "Buffer overflow for key: " << key;
                return false;
            }
            std::vector<uint8_t> value(leaf.size);
            std::memcpy(&value[0], inBuffer.data() + leaf.offset, leaf.size);

            // 转化成string
            std::string valString = dsf::parser::forwardToString(value, leaf.type);

            // 将构造好的数据放入outData中
            outData.emplace(key, valString);
        }

        return true;
    }

    size_t KeyValueSerializer::alignOffset(size_t offset, size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    KeyValueSerializer::~KeyValueSerializer()
    {
    }

} // namespace ngvs
} // namespace dsf
