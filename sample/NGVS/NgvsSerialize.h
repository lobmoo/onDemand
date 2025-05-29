/**
 * @file NgvsSerialize.h
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

#ifndef NGVS_SERIALIZE_H
#define NGVS_SERIALIZE_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "ModelParser.h"

namespace dsf
{
namespace ngvs
{
    class NgvsSerializer
    {
    public:
        explicit NgvsSerializer(std::map<std::string, ModelDefine> modelDefines, size_t alignment = 4);
        ~NgvsSerializer();
        

    void serialize();
    const std::vector<char>& buffer() const;


    private:
        size_t alignOffset(size_t offset, size_t alignment);
    private:
        std::vector<char> buffer_;
        std::vector<ModelDefine> sortedModels_;; 
        std::map<std::string, size_t> offsetMap_;
        size_t ALIGNMENT_; 
    };

} // namespace ac
} // namespace dsf

#endif // ac_ngvs_serialize_h