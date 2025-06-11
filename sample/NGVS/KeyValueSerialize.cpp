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
    bool KeyValueSerializer::serialize(std::string schema, const std::string ModelName, 
                                                std::unordered_map<std::string, char *> &data)
    {
        /* 1.调用ModelParser来解析所有model的结构 */
        parser::ModelParser parser;
        std::string error_message;

        parser::error_code_t ret = parser.parseSchema(modelDefines_, schema, error_message);
        if (ret != dsf::parser::MODEL_PARSER_OK) {
            LOG(error) << "parse schema failed, ret: " << ret;
            return false;
        }

        /* 2.判断解析的xml中是否含有该model */
        auto it = modelDefines_.find(ModelName);
        if (it == modelDefines_.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }
        parser::ModelDefine modelDefine = it->second;
        std::vector<parser::TreeNode> leaves;
        parser.findNodeAllLeaves(modelDefine, leaves);
        parser.printmembersInfo(leaves);

        

        /* 3. 申请一段空间(大小是固定的)并且顺序地将key-value对填入 */
        // buffer_ = new char[modelDefine.size]();
        // int idx = 0;
        // for(auto &keyvaluePair : data)
        // {
        //     const std::string &key = keyvaluePair.first;
        //     char *value = keyvaluePair.second;
        //     Result &memberInfo = modelDefine.members[idx];
        //     if(!serializeKeyValuePair(memberInfo, key, value))
        //     {
        //         LOG(error) << "Failed to serialize key-value pair: " << key;
        //         delBuffer();
        //         return false;
        //     }

        //     idx++; // 遍历modelDefine.members
        // } 

        return true;
    }

    bool KeyValueSerializer::serializeKeyValuePair(const parser::TreeNode &memberInfo, const std::string &key,
                                                    const char* value)
    {
        /* 1.判断键值对是否和遍历的members一致 */
        if(key != memberInfo.name) {
            // 写LOG(error) 还是thorw-catch
            LOG(error) << "Key mismatch: expected " << memberInfo.name << ", got " << key;
            return false;
        }

        /* 2.根据offset和size，填充buffer_ */
        if(memberInfo.offset + memberInfo.size > modelDefines_[memberInfo.name].size) {
            LOG(error) << "Buffer overflow for key: " << key;
            return false;
        }
        
        std::memcpy(buffer_ + memberInfo.offset, value, memberInfo.size); // 不用考虑对齐问题，因为已经在ModelParser中处理过了

        return true;
    }

    const char* KeyValueSerializer::buffer() const
    {
        return buffer_;
    }



    KeyValueSerializer::~KeyValueSerializer()
    {
    }

} // namespace ngvs
} // namespace dsf
