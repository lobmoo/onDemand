/**
 * @file NgvsSerialize.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-05-28
 * 
 * @copyright Copyright (c) 2025  by  瀹濅俊
 * 
 * @par 淇�鏀规棩蹇�:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-05-28     <td>1.0     <td>wwk   <td>淇�鏀�?
 * </table>
 */

#ifndef NGVS_SERIALIZE_H
#define NGVS_SERIALIZE_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_map>
#include "ModelParser.h"

namespace dsf
{
namespace ngvs
{
    using namespace dsf::parser;
    class NgvsSerializer
    {
    public:
        static NgvsSerializer &getInstance()
        {
            static NgvsSerializer instance;
            return instance;
        }
        ~NgvsSerializer();
        bool init(const std::string &schema);
        bool serialize(const std::string &ModelName, const std::vector<char> &inBuffer,
                       std::vector<char> &outBuffer);
        bool serialize(const std::string &ModelName,
                       const std::unordered_map<std::string, std::string> &inData,
                       std::vector<char> &outBuffer);
        bool deserialize(const std::string &ModelName, const std::vector<char> &inBuffer,
                         std::unordered_map<std::string, std::string> &outData);
        const std::vector<char> &buffer() const;

    private:
        NgvsSerializer();
        NgvsSerializer(const NgvsSerializer &) = delete;
        NgvsSerializer &operator=(const NgvsSerializer &) = delete;
        size_t alignOffset(size_t offset, size_t alignment);
        inline bool map2Buffer(const ModelDefine &model,
                               const std::unordered_map<std::string, std::string> &inData,
                               std::vector<char> &outBuffer);
        inline bool buffer2Map(const ModelDefine &model, const std::vector<char> &inBuffer,
                               std::unordered_map<std::string, std::string> &outData);
        void NgvsModelSort(ModelDefine &model);

    private:
        std::unordered_map<std::string, ModelDefine> modelDefines_;
        size_t ALIGNMENT_;
    };

} // namespace ngvs
} // namespace dsf

#endif // ac_ngvs_serialize_h