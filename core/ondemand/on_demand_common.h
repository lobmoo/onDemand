/**
 * @file on_demand_common.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2026-02-05
 * 
 * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2026-02-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */

#ifndef ON_DEMAND_COMMON_H
#define ON_DEMAND_COMMON_H

#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <condition_variable>

#include "dds_wrapper/dds_abstraction.h"
#include "dds_wrapper/dds_idl_wrapper.h"

namespace dsf
{
namespace ondemand
{


#define ONDEMAND_BUCKET_SIZE  20

/*日志宏定义*/
/************************************************************************************************ */
#define ONDEMANDLOG(level)                                                                         \
    LOG(level) << "["                                                                              \
               << "ON_DEMAND"                                                                      \
               << "] "

#define ONDEMANDLOG_TIME(level, interval_ms)                                                       \
    if (LogRateLimiter::shouldLog(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ":"     \
                                      + "ON_DEMAND",                                               \
                                  interval_ms))                                                    \
    ONDEMANDLOG(level)

    /************************************************************************************************** */

    /*bucket 管理类*/
    /************************************************************************************************** */
    class BucketManager
    {
    public:
        using Member = std::string;
        using BucketName = std::string;
        using BucketTable = std::unordered_map<BucketName, std::vector<Member>>;

        explicit BucketManager()
            : bucket_count_(ONDEMAND_BUCKET_SIZE == 0 ? 1 : ONDEMAND_BUCKET_SIZE)
        {
        }

        static BucketName CalculateBucketName(const Member &member)
        {
            const std::size_t idx =
                static_cast<std::size_t>(Fnv1aHash64(member) % ONDEMAND_BUCKET_SIZE);
            return "bucket_" + std::to_string(idx);
        }

        bool AddMember(const Member &member)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (member_to_bucket_.count(member)) {
                return false;
            }

            //  第一次添加时，创建固定数量的桶
            if (buckets_.empty()) {
                for (std::size_t i = 0; i < bucket_count_; ++i) {
                    buckets_[GetBucketName(i)] = std::vector<Member>{};
                }
            }

            //  桶数量固定
            const std::size_t idx = static_cast<std::size_t>(Fnv1aHash64(member) % bucket_count_);
            BucketName bucket_name = GetBucketName(idx);

            buckets_[bucket_name].push_back(member);
            member_to_bucket_[member] = bucket_name;
            return true;
        }

        bool RemoveMember(const Member &member)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = member_to_bucket_.find(member);
            if (it == member_to_bucket_.end()) {
                return false;
            }

            BucketName bucket_name = it->second;
            auto bucket_it = buckets_.find(bucket_name);
            if (bucket_it != buckets_.end()) {
                auto &members = bucket_it->second;
                members.erase(std::remove(members.begin(), members.end(), member), members.end());
            }

            member_to_bucket_.erase(it);
            return true;
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            member_to_bucket_.clear();
            buckets_.clear();
        }

        void Build(const std::vector<Member> &members)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            member_to_bucket_.clear();
            buckets_.clear();

            // 无论成员是否为空，都创建固定数量的桶
            std::vector<std::vector<Member>> temp_buckets(bucket_count_);
            for (const auto &name : members) {
                const std::size_t idx = static_cast<std::size_t>(Fnv1aHash64(name) % bucket_count_);
                temp_buckets[idx].push_back(name);
                member_to_bucket_[name] = GetBucketName(idx);
            }

            buckets_.reserve(bucket_count_);
            for (std::size_t i = 0; i < bucket_count_; ++i) {
                buckets_.emplace(GetBucketName(i), std::move(temp_buckets[i]));
            }
        }

        BucketName GetMemberBucket(const Member &member) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = member_to_bucket_.find(member);
            return it != member_to_bucket_.end() ? it->second : "";
        }

        BucketTable GetSnapshot() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return buckets_;
        }

        std::size_t GetBucketCount() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return buckets_.size();
        }

        std::size_t GetMemberCount() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return member_to_bucket_.size();
        }

        std::size_t GetBucketSize(const BucketName &bucket_name) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = buckets_.find(bucket_name);
            return it != buckets_.end() ? it->second.size() : 0;
        }

        std::vector<Member> GetBucketMembers(const BucketName &bucket_name) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = buckets_.find(bucket_name);
            return it != buckets_.end() ? it->second : std::vector<Member>{};
        }

        uint32_t GetTotalMembers() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return static_cast<uint32_t>(member_to_bucket_.size());
        }

        std::vector<BucketName> GetBucketNames() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<BucketName> names;
            names.reserve(buckets_.size());
            for (const auto &[name, _] : buckets_) {
                names.push_back(name);
            }
            std::sort(names.begin(), names.end(), [](const BucketName &a, const BucketName &b) {
                auto extract_num = [](const BucketName &s) -> std::size_t {
                    std::size_t pos = s.find('_');
                    if (pos != std::string::npos) {
                        return std::stoull(s.substr(pos + 1));
                    }
                    return 0;
                };
                return extract_num(a) < extract_num(b);
            });
            return names;
        }

        void PrintStats(std::ostream &os = std::cout) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            os << "Total members: " << member_to_bucket_.size()
               << ", Fixed bucket count: " << bucket_count_
               << ", Created buckets: " << buckets_.size() << "\n";
        }

        void PrintBuckets(std::ostream &os = std::cout) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            std::vector<std::pair<std::size_t, BucketName>> indexed_names;
            indexed_names.reserve(buckets_.size());

            for (const auto &[name, _] : buckets_) {
                std::size_t pos = name.find('_');
                std::size_t idx =
                    (pos != std::string::npos) ? std::stoull(name.substr(pos + 1)) : 0;
                indexed_names.emplace_back(idx, name);
            }

            std::sort(indexed_names.begin(), indexed_names.end());

            for (const auto &[idx, name] : indexed_names) {
                auto it = buckets_.find(name);
                if (it == buckets_.end())
                    continue;

                os << std::left << std::setw(12) << name << " (" << it->second.size() << "): ";
                for (const auto &member : it->second) {
                    os << member << " ";
                }
                os << "\n";
            }
        }

        std::size_t GetFixedBucketCount() const { return bucket_count_; }

    private:
        static uint64_t Fnv1aHash64(const std::string &s)
        {
            uint64_t h = 1469598103934665603ULL;
            for (unsigned char c : s) {
                h ^= c;
                h *= 1099511628211ULL;
            }
            return h;
        }

        BucketName GetBucketName(std::size_t idx) const { return "bucket_" + std::to_string(idx); }

        const std::size_t bucket_count_;
        BucketTable buckets_;
        std::unordered_map<Member, BucketName> member_to_bucket_;
        mutable std::mutex mutex_;
    };
    /*********************************************************************************************************** */

    /**
    * @brief FNV-1a 哈希函数
    */
    inline uint64_t fast_hash(const char *str)
    {
        uint64_t hash = 0xcbf29ce484222325ULL;
        while (*str) {
            hash ^= static_cast<uint64_t>(*str++);
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    inline uint64_t fast_hash(const std::string &str) { return fast_hash(str.c_str()); }

    /**
     * @brief 将字节数组转换为16进制字符串
     * @param data 数据指针
     * @param size 数据大小
     * @param maxBytes 最多打印的字节数（默认64）
     * @param includeAscii 是否包含ASCII视图（默认false）
     * @return 16进制字符串
     */
    inline std::string toHexString(const uint8_t *data, size_t size, size_t maxBytes = 64,
                                   bool includeAscii = false)
    {
        if (!data || size == 0) {
            return "<null>";
        }

        std::ostringstream oss;
        size_t printSize = std::min(size, maxBytes);

        for (size_t i = 0; i < printSize; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i])
                << " ";

            // 每16字节处理
            if ((i + 1) % 16 == 0) {
                if (includeAscii) {
                    oss << " | ";
                    for (size_t j = i - 15; j <= i; ++j) {
                        char c = static_cast<char>(data[j]);
                        oss << (std::isprint(c) ? c : '.');
                    }
                }
                if (i + 1 < printSize) {
                    oss << "\n  ";
                }
            }
        }

        // 处理最后不足16字节的情况
        size_t remainder = printSize % 16;
        if (remainder > 0 && includeAscii) {
            // 补齐空格
            for (size_t i = 0; i < (16 - remainder); ++i) {
                oss << "   ";
            }
            oss << " | ";
            for (size_t j = printSize - remainder; j < printSize; ++j) {
                char c = static_cast<char>(data[j]);
                oss << (std::isprint(c) ? c : '.');
            }
        }

        if (size > maxBytes) {
            oss << "\n  ... (+" << (size - maxBytes) << " more bytes)";
        }

        return oss.str();
    }

    template <typename TopicType, typename TopicPubSubType>
    int32_t registerNodeTopicWriter(
        std::shared_ptr<DdsWrapper::DataNode> node,
        std::shared_ptr<DdsWrapper::DDSTopicWriter<TopicType>> &dataWriter, // 输出引用
        const std::string &dsfTopicName, const DdsWrapper::DataWriterQoSBuilder &writer_qos,
        DdsWrapper::DataWriterListener *listener = nullptr)
    {
        dataWriter = node->template createDataWriter<TopicType, TopicPubSubType>(
            dsfTopicName, writer_qos, listener);
        if (!dataWriter) {
            LOG(error) << "Failed to create topic writer for topic [" << dsfTopicName << "].";
            return -1;
        }

        LOG(info) << "Topic [" << dsfTopicName << "] writer created.";
        return 0;
    }

    template <typename TopicType, typename TopicPubSubType>
    int32_t registerNodeTopicReader(
        std::shared_ptr<DdsWrapper::DataNode> node,
        std::shared_ptr<DdsWrapper::DDSTopicReader<TopicType>> &dataReader, // 输出引用
        const std::string &dsfTopicName,
        std::function<void(const std::string &, std::shared_ptr<TopicType>)> processFunc,
        const DdsWrapper::DataReaderQoSBuilder &reader_qos,
        DdsWrapper::DataReaderListener<TopicType> *listener = nullptr)
    {

        dataReader = node->template createDataReader<TopicType, TopicPubSubType>(
            dsfTopicName, processFunc, reader_qos, listener);
        if (!dataReader) {
            LOG(error) << "Failed to create topic reader for topic [" << dsfTopicName << "].";
            return -1;
        }

        LOG(debug) << "Topic [" << dsfTopicName << "] reader created.";
        return 0;
    }

} // namespace ondemand
} // namespace dsf
#endif // ON_DEMAND_COMMON_H