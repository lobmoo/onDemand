/**
 * @file NgvsSerialize.cpp
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-05-28
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par �?改日�?:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-05-28     <td>1.0     <td>wwk   <td>�?�??
 * </table>
 */
#include "NgvsSerialize.h"
#include "log/logger.h"
#include <cmath>

namespace dsf
{
namespace ngvs
{

    NgvsSerializer::NgvsSerializer(size_t alignment) : ALIGNMENT_(alignment)
    {
    }

    bool NgvsSerializer::serialize(std::string schema, const std::string ModelName)
    {
        ModelParser parser;
        std::string error_message;

        /*1.解析xml数据*/
        error_code_t ret = parser.parseSchema(modelDefines_, schema, error_message);
        if (ret != dsf::ngvs::MODEL_PARSER_OK) {
            LOG(error) << "parse schema failed, ret: " << ret;
            return false;
        }

        /*2.解析model下面的数�?类型*/
        auto it = modelDefines_.find(ModelName);
        if (it == modelDefines_.end()) {
            LOG(error) << "model not found: " << ModelName;
            return false;
        }

        // /*找到数据类型，开始�?�找根节点的数据类型以及计算偏移*/
        ModelDefine modelDefine = it->second;
        LOG(info) << "----------------------------/n";
        LOG(info) << "model name: " << modelDefine.modelName
                  << "  model name: " << modelDefine.modelVersion
                  << "  model size: " << modelDefine.size;
        for (auto &ptr : modelDefine.members) {
            LOG(info) << "ele name: " << ptr.name << "  ele type: " << ptr.type
                      << "  ele size: " << ptr.size << "  ele offset: " << ptr.offset;
        }
        LOG(info) << "----------------------------/n";

        /*找出model下面的结构*/
        std::vector<std::string> modekeys;
        // for (auto &ptr : modelDefine.members)
        // {
        //     if()
        // }

            return 0;
    }

    std::vector<Result>
    NgvsSerializer::calculateStructSize(const std::map<std::string, std::string> &data)
    {
    }

    const std::vector<char> &NgvsSerializer::buffer() const
    {
        return buffer_;
    }

    size_t NgvsSerializer::alignOffset(size_t offset, size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    NgvsSerializer::~NgvsSerializer()
    {
    }

} // namespace ngvs
} // namespace dsf
