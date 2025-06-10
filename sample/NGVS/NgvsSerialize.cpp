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

#define NGVS_DEBUG

namespace dsf
{
namespace ngvs
{
    using namespace dsf::parser;
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
        ModelDefine model = it->second;

#ifdef NGVS_DEBUG
        parser.printAllLeafNodesInfo(model);
#endif

            return 0;
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
