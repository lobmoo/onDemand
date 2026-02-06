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
#include <unordered_set>
#include <condition_variable>

#include "dds_wrapper/dds_abstraction.h"
#include "dds_wrapper/dds_idl_wrapper.h"

namespace dsf
{
namespace ondemand
{

#define ONDEMAND_BUCKET_SIZE 20
#define DOMAIN_ID 66

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

    inline uint64_t fast_hash(const char *str);
    inline uint64_t fast_hash(const std::string &str);

    /*bucket 管理类*/
    /************************************************************************************************** */
    class BucketManager
    {
    public:
        using Member = std::string;
        using BucketIndex = std::size_t;
        using BucketTable = std::vector<std::unordered_set<Member>>;

        explicit BucketManager()
            : bucket_count_(ONDEMAND_BUCKET_SIZE == 0 ? 1 : ONDEMAND_BUCKET_SIZE), buckets_(),
              total_members_(0), mutex_()
        {
            buckets_.resize(bucket_count_);
        }

        static BucketIndex CalculateBucketIndex(const Member &member)
        {
            return static_cast<BucketIndex>(fast_hash(member) % ONDEMAND_BUCKET_SIZE);
        }


        static BucketIndex CalculateBucketIndexFromHash(uint64_t hash)
        {
            return static_cast<BucketIndex>(hash % ONDEMAND_BUCKET_SIZE);
        }

       
        bool AddMember(const Member &member, uint64_t hash)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            const BucketIndex idx = static_cast<BucketIndex>(hash % bucket_count_);
            auto &bucket = buckets_[idx];
            
            auto [it, inserted] = bucket.insert(member);
            if (inserted) {
                ++total_members_;
            }
            return inserted;
        }

        
        bool AddMember(const Member &member) { return AddMember(member, fast_hash(member)); }

      
        bool RemoveMember(const Member &member, uint64_t hash)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            const BucketIndex idx = static_cast<BucketIndex>(hash % bucket_count_);
            auto &bucket = buckets_[idx];
            
            
            size_t erased = bucket.erase(member);
            if (erased > 0) {
                --total_members_;
                return true;
            }
            return false;
        }

       
        bool RemoveMember(const Member &member) { return RemoveMember(member, fast_hash(member)); }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto &bucket : buckets_) {
                bucket.clear();
            }
            total_members_ = 0;
        }

        void Build(const std::vector<Member> &members)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto &bucket : buckets_) {
                bucket.clear();
            }

            for (const auto &name : members) {
                const BucketIndex idx = static_cast<BucketIndex>(fast_hash(name) % bucket_count_);
                buckets_[idx].insert(name);
            }

            total_members_ = members.size();
        }

        BucketIndex GetMemberBucket(const Member &member) const
        {
            // 直接计算，无需查表
            return CalculateBucketIndex(member);
        }

        bool HasMember(const Member &member) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const BucketIndex idx = CalculateBucketIndex(member);
            const auto &bucket = buckets_[idx];
            return bucket.find(member) != bucket.end();
        }

        BucketTable GetSnapshot() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return buckets_;
        }

        std::size_t GetBucketCount() const { return bucket_count_; }

        std::size_t GetMemberCount() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return total_members_;
        }

        std::size_t GetBucketSize(BucketIndex idx) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return idx < buckets_.size() ? buckets_[idx].size() : 0;
        }

        std::vector<Member> GetBucketMembers(BucketIndex idx) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (idx >= buckets_.size()) {
                return std::vector<Member>{};
            }
            const auto &bucket = buckets_[idx];
            return std::vector<Member>(bucket.begin(), bucket.end());
        }

        uint32_t GetTotalMembers() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return static_cast<uint32_t>(total_members_);
        }

        std::vector<BucketIndex> GetBucketIndices() const
        {
            std::vector<BucketIndex> indices;
            indices.reserve(bucket_count_);
            for (BucketIndex i = 0; i < bucket_count_; ++i) {
                indices.push_back(i);
            }
            return indices;
        }

        void PrintStats(std::ostream &os = std::cout) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            os << "Total members: " << total_members_ << ", Fixed bucket count: " << bucket_count_
               << "\n";
        }

        void PrintBuckets(std::ostream &os = std::cout) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            for (BucketIndex i = 0; i < buckets_.size(); ++i) {
                os << "bucket_" << i << " (" << buckets_[i].size() << "): ";
                for (const auto &member : buckets_[i]) {
                    os << member << " ";
                }
                os << "\n";
            }
        }

    private:
        const std::size_t bucket_count_;
        BucketTable buckets_;
        std::size_t total_members_;
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