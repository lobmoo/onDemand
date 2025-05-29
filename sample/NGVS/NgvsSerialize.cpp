/**
 * @file NgvsSerialize.cpp
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-05-28
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-05-28     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#include "NgvsSerialize.h"

namespace dsf
{
namespace ngvs
{

    NgvsSerializer::NgvsSerializer(std::map<std::string, ModelDefine> modelDefines,
                                   size_t alignment)
        : buffer_(), ALIGNMENT_(alignment)
    {
        size_t currentOffset = 0;
        /*1.先保存一份副本*/
        for (auto &model : modelDefines) {
            ModelDefine define = model.second;
            sortedModels_.push_back(define);
        }
        /*2.然后进行排序*/
        std::sort(sortedModels_.begin(), sortedModels_.end(),
                  [](const ModelDefine &a, const ModelDefine &b) { return a.size > b.size; });
        /*3.字节对齐一下*/
        for (const auto &model : sortedModels_) {
            currentOffset = alignOffset(currentOffset, ALIGNMENT_);
            offsetMap_[model.modelName] = currentOffset;
            currentOffset += model.size;
        }
        /*4.构造一块内存 用来放数据*/    
        buffer_.resize(currentOffset, 0);
    }

    void NgvsSerializer::serialize()
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
