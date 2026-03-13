/**
 * @file variable_store.h
 * @brief
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026  by  wwk : wwk.lobmo@gmail.com
 *
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2026-03-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */

#ifndef VARIABLE_STORE_H
#define VARIABLE_STORE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "concurrentqueue.h"

namespace dsf
{
namespace ondemand
{

    inline constexpr uint32_t align64(uint32_t n) { return (n + 63u) & ~63u; }

    struct VarMeta {
        uint32_t id;
        uint32_t offset;
        uint32_t size;
    };

    /**
     * @brief 双缓冲 Slot
     *
     * 内存布局（arena 内）:
     *   [seq(4B) + committed(1B) + pad(59B)] [buf0: size B] [buf1: size B]
     *
     * 写端始终写 1-committed 的那份，写完后原子翻转 committed。
     * 读端读 committed 指向的那份，用 seqlock 验证读取期间无翻转。
     */
    struct alignas(64) Slot {
        std::atomic<uint32_t> seq{0};       // seqlock 计数器，奇数=写入中
        std::atomic<uint8_t>  committed{0}; // 当前可读缓冲区索引 (0 或 1)
        std::byte data[];                   // [buf0: size B][buf1: size B]
    };

    class VarStore
    {
    public:
        static constexpr uint32_t kInvalidId = ~0u;

        VarStore() = default;
        ~VarStore() { cleanup(); }
        VarStore(const VarStore &) = delete;
        VarStore &operator=(const VarStore &) = delete;

        uint32_t register_var(uint64_t hash, uint32_t size)
        {
            ConfigGuard g(this);
            auto it = table_.find(hash);
            if (it != table_.end())
                return (it->second < metas_.size() && metas_[it->second].size == size) ? it->second
                                                                                       : kInvalidId;
            uint32_t id = metas_.size();
            table_[hash] = id;
            metas_.push_back({id, arena_size_, size});
            /*提高内存访问效率，减少伪共享和cache line冲突，适配现代CPU缓存（cache line一般为64字节）。
                保证Slot结构和数据在多线程环境下原子操作时不会跨cache line，避免false sharing。
                有些SIMD/原子操作要求对齐，防止未对齐访问导致性能下降或异常。*/
            arena_size_ += align64(sizeof(Slot) + size);
            return id;
        }

        uint32_t get_id(uint64_t hash) const
        {
            auto it = table_.find(hash);
            return (it != table_.end()) ? it->second : kInvalidId;
        }

        /*这里是软删除，不会真的删除，但是总大小可控，目前先这样*/
        bool unregister_var(uint64_t hash)
        {
            ConfigGuard g(this);
            auto it = table_.find(hash);
            if (it == table_.end())
                return false;
            uint32_t id = it->second;
            if (id >= metas_.size())
                return false;
            metas_[id].size = 0; // 软删除: 标记为无效
            table_.erase(it);
            return true;
        }

        bool finalize()
        {
            ConfigGuard g(this);
            uint32_t new_count = metas_.size();
            /*防止越界*/
            if (new_count <= var_count_)
                return true;

            arena_size_ = align64(arena_size_);
            uint32_t old_size = var_count_
                                    ? metas_[var_count_ - 1].offset
                                          + align64(sizeof(Slot) + 2 * metas_[var_count_ - 1].size)
                                    : 0;

            void *new_arena = std::aligned_alloc(64, arena_size_);
            if (!new_arena)
                return false;

            /*这里支持动态扩容*/
            if (old_size)
                std::memcpy(new_arena, arena_, old_size);
            if (arena_size_ > old_size)
                std::memset((std::byte *)new_arena + old_size, 0, arena_size_ - old_size);
            std::free(arena_);
            arena_ = (std::byte *)new_arena;

            auto *new_flags = new (std::nothrow) std::atomic<bool>[new_count];
            if (!new_flags)
                return false;

            /*扩容脏队列*/
            for (uint32_t i = 0; i < var_count_; ++i)
                new_flags[i].store(dirty_flags_[i].load(std::memory_order_relaxed),
                                   std::memory_order_relaxed);
            for (uint32_t i = var_count_; i < new_count; ++i)
                new_flags[i].store(false, std::memory_order_relaxed);
            delete[] dirty_flags_;
            dirty_flags_ = new_flags;
            var_count_ = new_count;
            return true;
        }

        // ---- 写接口 ----

        bool write(uint32_t id, const void *src)
        {
            OpGuard g(this);
            if (!arena_ || !dirty_flags_ || id >= var_count_)
                return false;
            auto *slot = (Slot *)(arena_ + metas_[id].offset);
            uint32_t size = metas_[id].size;

            slot->seq.fetch_add(1, std::memory_order_acquire);
            uint8_t write_idx = 1u - slot->committed.load(std::memory_order_relaxed);
            std::memcpy(slot->data + write_idx * size, src, size);
            slot->committed.store(write_idx, std::memory_order_release);
            slot->seq.fetch_add(1, std::memory_order_release);

            if (!dirty_flags_[id].exchange(true, std::memory_order_acq_rel))
                dirty_queue_.enqueue(id);
            return true;
        }

        bool write(uint32_t id, const void *src, uint32_t actual_size)
        {
            OpGuard g(this);
            if (!arena_ || !dirty_flags_ || id >= var_count_ || metas_[id].size == 0)
                return false;
            if (actual_size > metas_[id].size)
                return false;
            auto *slot = (Slot *)(arena_ + metas_[id].offset);
            uint32_t size = metas_[id].size;

            slot->seq.fetch_add(1, std::memory_order_acquire);
            uint8_t write_idx = 1u - slot->committed.load(std::memory_order_relaxed);
            std::memcpy(slot->data + write_idx * size, src, actual_size);
            slot->committed.store(write_idx, std::memory_order_release);
            slot->seq.fetch_add(1, std::memory_order_release);

            if (!dirty_flags_[id].exchange(true, std::memory_order_acq_rel))
                dirty_queue_.enqueue(id);
            return true;
        }

        /**
         * @brief 批量写，一次 op_enter 覆盖所有变量，热路径首选
         * @param ids   变量 id 数组
         * @param datas 对应数据指针数组
         * @param sizes 对应数据大小数组
         * @param count 数量
         */
        void write_batch(const uint32_t *ids, const void *const *datas,
                         const uint32_t *sizes, size_t count)
        {
            OpGuard g(this);
            if (!arena_ || !dirty_flags_) return;
            for (size_t i = 0; i < count; ++i) {
                uint32_t id = ids[i];
                if (id >= var_count_ || metas_[id].size == 0) continue;
                auto *slot = (Slot *)(arena_ + metas_[id].offset);
                uint32_t size = metas_[id].size;
                uint32_t actual = sizes[i] <= size ? sizes[i] : size;

                slot->seq.fetch_add(1, std::memory_order_acquire);
                uint8_t write_idx = 1u - slot->committed.load(std::memory_order_relaxed);
                std::memcpy(slot->data + write_idx * size, datas[i], actual);
                slot->committed.store(write_idx, std::memory_order_release);
                slot->seq.fetch_add(1, std::memory_order_release);

                if (!dirty_flags_[id].exchange(true, std::memory_order_acq_rel))
                    dirty_queue_.enqueue(id);
            }
        }

        // ---- 读接口 ----

        /**
         * @brief 带 memcpy 的安全读（兼容旧接口）
         */
        bool read(uint32_t id, void *dst) const
        {
            OpGuard g(this);
            if (!arena_ || id >= var_count_)
                return false;
            auto *slot = (const Slot *)(arena_ + metas_[id].offset);
            uint32_t size = metas_[id].size;
            for (int retry = 0; retry < 10; ++retry) {
                uint32_t s1 = slot->seq.load(std::memory_order_acquire);
                if (s1 & 1) continue;
                uint8_t idx = slot->committed.load(std::memory_order_acquire);
                std::memcpy(dst, slot->data + idx * size, size);
                if (s1 == slot->seq.load(std::memory_order_acquire))
                    return true;
            }
            return false;
        }

        /**
         * @brief 零拷贝读句柄
         *
         * 构造时验证 seqlock，成功后持有 op_enter() 防止 arena 重分配。
         * ptr() 直接指向 arena 内 committed 缓冲区，无 memcpy。
         * 析构时释放 op_exit()。
         */
        class ZeroCopyReadHandle
        {
            const VarStore  *s_;
            const std::byte *ptr_;
            uint32_t         size_;

        public:
            ZeroCopyReadHandle(const VarStore *s, uint32_t id) : s_(s), ptr_(nullptr), size_(0)
            {
                if (!s_) return;
                s_->op_enter();
                if (!s_->arena_ || id >= s_->var_count_) return;

                auto *slot = (const Slot *)(s_->arena_ + s_->metas_[id].offset);
                uint32_t size = s_->metas_[id].size;

                for (int retry = 0; retry < 10; ++retry) {
                    uint32_t s1 = slot->seq.load(std::memory_order_acquire);
                    if (s1 & 1) continue;
                    uint8_t idx = slot->committed.load(std::memory_order_acquire);
                    std::atomic_thread_fence(std::memory_order_acquire);
                    if (s1 == slot->seq.load(std::memory_order_acquire)) {
                        ptr_  = slot->data + idx * size;
                        size_ = size;
                        return;
                    }
                }
            }

            ~ZeroCopyReadHandle() { if (s_) s_->op_exit(); }

            ZeroCopyReadHandle(ZeroCopyReadHandle &&o) noexcept
                : s_(o.s_), ptr_(o.ptr_), size_(o.size_) { o.s_ = nullptr; }
            ZeroCopyReadHandle(const ZeroCopyReadHandle &) = delete;

            const std::byte *ptr()  const { return ptr_; }
            uint32_t         size() const { return size_; }
            explicit operator bool() const { return ptr_ != nullptr; }
        };

        ZeroCopyReadHandle read_zero_copy(uint32_t id) const { return {this, id}; }

        template <typename Fn>
        void for_each_dirty(Fn &&fn, size_t batch = 1000)
        {
            std::vector<uint32_t> ids;
            ids.reserve(batch);
            {
                OpGuard g(this);
                dirty_queue_.try_dequeue_bulk(std::back_inserter(ids), batch);
                for (auto id : ids)
                    if (id < var_count_)
                        dirty_flags_[id].store(false, std::memory_order_release);
            }
            for (auto id : ids)
                fn(id);
        }

        uint32_t var_count() const { return var_count_; }
        uint32_t total_size() const { return arena_size_; }
        size_t queue_size_approx() const { return dirty_queue_.size_approx(); }

    private:
        struct OpGuard {
            const VarStore *s_;
            OpGuard(const VarStore *s) : s_(s) { s_->op_enter(); }
            ~OpGuard() { s_->op_exit(); }
        };

        struct ConfigGuard {
            VarStore *s_;
            ConfigGuard(VarStore *s) : s_(s) { s_->config_begin(); }
            ~ConfigGuard() { s_->config_end(); }
        };

        void op_enter() const
        {
            for (;;) {
                while (reconfig_.load(std::memory_order_acquire))
                    std::this_thread::yield();
                active_ops_.fetch_add(1, std::memory_order_acq_rel);
                if (!reconfig_.load(std::memory_order_acquire))
                    return;
                active_ops_.fetch_sub(1, std::memory_order_acq_rel);
            }
        }
        void op_exit() const { active_ops_.fetch_sub(1, std::memory_order_acq_rel); }

        void config_begin()
        {
            config_mu_.lock();
            reconfig_.store(true, std::memory_order_release);
            while (active_ops_.load(std::memory_order_acquire))
                std::this_thread::yield();
        }
        void config_end()
        {
            reconfig_.store(false, std::memory_order_release);
            config_mu_.unlock();
        }

        void cleanup()
        {
            std::free(arena_);
            delete[] dirty_flags_;
            arena_ = nullptr;
            dirty_flags_ = nullptr;
        }

        std::mutex config_mu_;
        std::atomic<bool> reconfig_{false};
        mutable std::atomic<uint32_t> active_ops_{0};

        std::unordered_map<uint64_t, uint32_t> table_;
        std::vector<VarMeta> metas_;
        uint32_t arena_size_ = 0, var_count_ = 0;
        std::byte *arena_ = nullptr;
        std::atomic<bool> *dirty_flags_ = nullptr;
        moodycamel::ConcurrentQueue<uint32_t> dirty_queue_;
    };
} // namespace ondemand
} // namespace dsf

#endif // VARIABLE_STORE_H
