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
     *   [seq(4B) + committed(1B) + valid_size(8B) + pad(51B)] = 64B header
     *   [buf0: size B][buf1: size B]
     *
     * 写端始终写 1-committed 的那份，写完后原子翻转 committed。
     * 读端读 committed 指向的那份，用 seqlock 验证读取期间无翻转。
     *
     * Seqlock 内存序协议（P0-1 修复）：
     *   写开始: fetch_add(relaxed) + fence(release) — 阻止后续 memcpy 上移到 seq++ 之前
     *   写结束: fetch_add(release)                  — 让读端 acquire-load 可见 memcpy 结果
     *   读端:   seq.load(acquire)                   — 与写结束 release 配对
     */
    struct alignas(64) Slot {
        std::atomic<uint32_t> seq{0};       // seqlock 计数器，奇数=写入中
        std::atomic<uint8_t>  committed{0}; // 当前可读缓冲区索引 (0 或 1)
        uint32_t valid_size[2]{0, 0};       // 每个缓冲区的有效数据长度 (8B)
        uint8_t pad_[48]{};
        std::byte data[];
    };
    static_assert(alignof(Slot) >= 64, "Slot alignment insufficient");
    static_assert(sizeof(Slot) == 64, "Slot header must be exactly 64 bytes");

    inline constexpr uint32_t slot_bytes(uint32_t payload_size) {
        return align64(static_cast<uint32_t>(sizeof(Slot)) + 2u * payload_size);
    }

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
                return (it->second < metas_.size() && metas_[it->second].size == size)
                           ? it->second
                           : kInvalidId;
            uint32_t id = metas_.size();
            table_[hash] = id;
            metas_.push_back({id, arena_size_, size});
            /*提高内存访问效率，减少伪共享和cache line冲突，适配现代CPU缓存（cache line一般为64字节）。
                保证Slot结构和数据在多线程环境下原子操作时不会跨cache line，避免false sharing。
                有些SIMD/原子操作要求对齐，防止未对齐访问导致性能下降或异常。*/
            arena_size_ += slot_bytes(size);
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
            uint32_t new_count = static_cast<uint32_t>(metas_.size());
            /*防止越界*/
            if (new_count <= var_count_)
                return true;

            arena_size_ = align64(arena_size_);
            uint32_t old_size = var_count_
                                    ? metas_[var_count_ - 1].offset + slot_bytes(metas_[var_count_ - 1].size)
                                    : 0;

            void *new_arena = std::aligned_alloc(64, arena_size_);
            if (!new_arena) {
                ONDEMANDLOG(error) << "Failed to allocate arena (aligned_alloc), size: " << arena_size_;
                return false;
            }

            /*这里支持动态扩容*/
            if (old_size)
                std::memcpy(new_arena, arena_, old_size);
            if (arena_size_ > old_size)
                std::memset((std::byte *)new_arena + old_size, 0, arena_size_ - old_size);

            auto *new_flags = new (std::nothrow) std::atomic<bool>[new_count];
            if (!new_flags) {
                /* 分配 dirty_flags 失败：new_arena 尚未赋给 arena_，直接释放，保持原状态一致 */
                std::free(new_arena);
                return false;
            }

            std::free(arena_);
            arena_ = (std::byte *)new_arena;

            /*扩容脏队列*/
            for (uint32_t i = 0; i < var_count_; ++i)
                new_flags[i].store(dirty_flags_[i].load(std::memory_order_relaxed),
                                   std::memory_order_relaxed);
            for (uint32_t i = var_count_; i < new_count; ++i)
                new_flags[i].store(false, std::memory_order_relaxed);
            auto *old_flags = dirty_flags_;
            dirty_flags_ = new_flags;
            var_count_ = new_count;
            delete[] old_flags;  // Safe: only after pointers updated
            return true;
        }

        // ---- 写接口 ----

        bool write(uint32_t id, const void *src)
        {
            OpGuard g(this);
            if (!arena_ || !dirty_flags_ || id >= var_count_ || metas_[id].size == 0 || !src)
                return false;
            auto *slot = (Slot *)(arena_ + metas_[id].offset);
            uint32_t size = metas_[id].size;

            // [P0-1] 写开始: relaxed fetch_add + release fence，阻止 memcpy 上移到 seq++ 之前
            slot->seq.fetch_add(1, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_release);
            uint8_t write_idx = 1u - slot->committed.load(std::memory_order_relaxed);
            std::memcpy(slot->data + write_idx * size, src, size);
            slot->valid_size[write_idx] = size;
            slot->committed.store(write_idx, std::memory_order_release);
            slot->seq.fetch_add(1, std::memory_order_release);

            if (!dirty_flags_[id].exchange(true, std::memory_order_acq_rel))
                dirty_queue_.enqueue(id);
            return true;
        }

        bool write(uint32_t id, const void *src, uint32_t actual_size)
        {
            if (!src || actual_size == 0)
                return false;

            for (;;) {
                {
                    OpGuard g(this);
                    if (!arena_ || !dirty_flags_ || id >= var_count_ || metas_[id].size == 0)
                        return false;

                    uint32_t size = metas_[id].size;
                    if (actual_size <= size) {
                        auto *slot = (Slot *)(arena_ + metas_[id].offset);
                        slot->seq.fetch_add(1, std::memory_order_relaxed);
                        std::atomic_thread_fence(std::memory_order_release);
                        uint8_t write_idx = 1u - slot->committed.load(std::memory_order_relaxed);
                        std::memcpy(slot->data + write_idx * size, src, actual_size);
                        slot->valid_size[write_idx] = actual_size;
                        slot->committed.store(write_idx, std::memory_order_release);
                        slot->seq.fetch_add(1, std::memory_order_release);

                        if (!dirty_flags_[id].exchange(true, std::memory_order_acq_rel))
                            dirty_queue_.enqueue(id);
                        return true;
                    }
                }

                if (!ensure_capacity(id, actual_size))
                    return false;
            }
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
            if (!ids || !datas || !sizes)
                return;

            /* 先按需扩容（不持 OpGuard，避免与 config_begin 死锁），
             * 扩容后在单次 OpGuard 内完成所有写入，消除两次 OpGuard 之间
             * offset 被其他线程扩容改变的窗口。*/
            for (size_t i = 0; i < count; ++i) {
                uint32_t id = ids[i];
                if (id == kInvalidId || !datas[i])
                    continue;

                bool need_expand = false;
                {
                    OpGuard g(this);
                    if (!arena_ || id >= var_count_ || metas_[id].size == 0)
                        continue;
                    need_expand = sizes[i] > metas_[id].size;
                }

                if (need_expand)
                    (void)ensure_capacity(id, sizes[i]);
            }

            OpGuard g(this);
            if (!arena_ || !dirty_flags_)
                return;
            for (size_t i = 0; i < count; ++i) {
                uint32_t id = ids[i];
                if (id >= var_count_ || metas_[id].size == 0 || !datas[i])
                    continue;
                /* 在同一个 OpGuard 内读取 offset，保证与扩容后的 metas_ 一致 */
                auto *slot = (Slot *)(arena_ + metas_[id].offset);
                uint32_t size = metas_[id].size;
                uint32_t actual = sizes[i] <= size ? sizes[i] : size;

                slot->seq.fetch_add(1, std::memory_order_relaxed);
                std::atomic_thread_fence(std::memory_order_release);
                uint8_t write_idx = 1u - slot->committed.load(std::memory_order_relaxed);
                std::memcpy(slot->data + write_idx * size, datas[i], actual);
                slot->valid_size[write_idx] = actual;
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
            /* op_enter() 已防止 arena 重分配，writer 必然完成，可安全自旋 */
            for (;;) {
                uint32_t s1 = slot->seq.load(std::memory_order_acquire);
                if (s1 & 1) {
                    std::this_thread::yield();
                    continue;
                }
                uint8_t idx = slot->committed.load(std::memory_order_acquire);
                uint32_t actual = slot->valid_size[idx];
                if (actual == 0 || actual > size)
                    actual = size;
                std::memcpy(dst, slot->data + idx * size, actual);
                if (s1 == slot->seq.load(std::memory_order_acquire))
                    return true;
            }
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
            bool             held_;

        public:
            ZeroCopyReadHandle(const VarStore *s, uint32_t id)
                : s_(s), ptr_(nullptr), size_(0), held_(false)
            {
                if (!s_)
                    return;
                s_->op_enter();
                held_ = true;
                if (!s_->arena_ || id >= s_->var_count_)
                    return;

                auto *slot = (const Slot *)(s_->arena_ + s_->metas_[id].offset);
                uint32_t size = s_->metas_[id].size;

                /* op_enter() 已防止 arena 重分配，writer 必然完成，可安全自旋 */
                for (;;) {
                    uint32_t s1 = slot->seq.load(std::memory_order_acquire);
                    if (s1 & 1) {
                        std::this_thread::yield();
                        continue;
                    }
                    uint8_t idx = slot->committed.load(std::memory_order_acquire);
                    std::atomic_thread_fence(std::memory_order_acquire);
                    if (s1 == slot->seq.load(std::memory_order_acquire)) {
                        uint32_t actual = slot->valid_size[idx];
                        if (actual == 0 || actual > size)
                            actual = size;
                        ptr_ = slot->data + idx * size;
                        size_ = actual;
                        return;
                    }
                }
            }

            ~ZeroCopyReadHandle()
            {
                if (s_ && held_)
                    s_->op_exit();
            }

            ZeroCopyReadHandle(ZeroCopyReadHandle &&o) noexcept
                : s_(o.s_), ptr_(o.ptr_), size_(o.size_), held_(o.held_)
            {
                o.ptr_ = nullptr;  // 标记原对象已失效，析构时不 op_exit
                o.held_ = false;
            }
            ZeroCopyReadHandle(const ZeroCopyReadHandle &) = delete;

            const std::byte *ptr() const { return ptr_; }
            uint32_t size() const { return size_; }
            explicit operator bool() const { return ptr_ != nullptr; }
        };

        ZeroCopyReadHandle read_zero_copy(uint32_t id) const { return {this, id}; }

        /**
         * @brief 批量零拷贝读，一次 op_enter 覆盖所有变量，减少锁开销
         * @param ids    变量 id 数组
         * @param count  数量
         * @param fn     回调 fn(i, ptr, size)，ptr==nullptr 表示该变量无数据
         */
        template <typename Fn>
        void read_batch(const uint32_t *ids, size_t count, Fn &&fn) const
        {
            OpGuard g(this);
            if (!arena_ || !dirty_flags_)
                return;
            for (size_t i = 0; i < count; ++i) {
                uint32_t id = ids[i];
                if (id >= var_count_ || metas_[id].size == 0) {
                    fn(i, nullptr, 0u);
                    continue;
                }
                auto *slot = (const Slot *)(arena_ + metas_[id].offset);
                uint32_t size = metas_[id].size;
                /* seqlock 自旋读，op_enter 已防止 arena 重分配 */
                for (;;) {
                    uint32_t s1 = slot->seq.load(std::memory_order_acquire);
                    if (s1 & 1) { std::this_thread::yield(); continue; }
                    uint8_t idx = slot->committed.load(std::memory_order_acquire);
                    std::atomic_thread_fence(std::memory_order_acquire);
                    if (s1 == slot->seq.load(std::memory_order_acquire)) {
                        uint32_t actual = slot->valid_size[idx];
                        if (actual == 0 || actual > size) actual = size;
                        fn(i, reinterpret_cast<const void *>(slot->data + idx * size), actual);
                        break;
                    }
                }
            }
        }

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

        uint32_t slot_size(uint32_t id) const
        {
            OpGuard g(this);
            if (!arena_ || id >= var_count_)
                return 0;
            return metas_[id].size;
        }

    private:
        mutable std::mutex reconfig_mu_;
        mutable std::condition_variable reconfig_cv_;

        bool ensure_capacity(uint32_t id, uint32_t required)
        {
            if (required == 0 || id >= metas_.size())
                return false;

            ConfigGuard g(this);
            if (id >= metas_.size() || metas_[id].size == 0)
                return false;

            uint32_t old_size = metas_[id].size;
            if (required <= old_size)
                return true;

            uint32_t new_size = required;
            uint32_t doubled = old_size > (UINT32_MAX / 2u) ? old_size : old_size * 2u;
            if (doubled > new_size)
                new_size = doubled;

            std::vector<uint32_t> new_offsets(metas_.size(), 0u);
            uint32_t new_arena_size = 0;
            for (size_t i = 0; i < metas_.size(); ++i) {
                new_offsets[i] = new_arena_size;
                uint32_t sz = (static_cast<uint32_t>(i) == id) ? new_size : metas_[i].size;
                new_arena_size += slot_bytes(sz);
            }
            new_arena_size = align64(new_arena_size);

            void *new_arena = std::aligned_alloc(64, new_arena_size);
            if (!new_arena)
                return false;
            std::memset(new_arena, 0, new_arena_size);

            if (arena_) {
                for (size_t i = 0; i < metas_.size(); ++i) {
                    uint32_t src_size = metas_[i].size;
                    uint32_t dst_size = (static_cast<uint32_t>(i) == id) ? new_size : src_size;

                    auto *src_slot = reinterpret_cast<Slot *>(arena_ + metas_[i].offset);
                    auto *dst_slot = reinterpret_cast<Slot *>(reinterpret_cast<std::byte *>(new_arena)
                                                              + new_offsets[i]);

                    dst_slot->seq.store(src_slot->seq.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
                    dst_slot->committed.store(src_slot->committed.load(std::memory_order_relaxed),
                                              std::memory_order_relaxed);
                    dst_slot->valid_size[0] = src_slot->valid_size[0];
                    dst_slot->valid_size[1] = src_slot->valid_size[1];

                    // 分别拷贝两个缓冲区，避免越界
                    uint32_t copy_size = src_size < dst_size ? src_size : dst_size;
                    if (copy_size > 0) {
                        std::memcpy(dst_slot->data, src_slot->data, copy_size);  // buf0
                        std::memcpy(dst_slot->data + dst_size, src_slot->data + src_size, copy_size);  // buf1
                    }
                }
            }

            std::free(arena_);
            arena_ = reinterpret_cast<std::byte *>(new_arena);
            metas_[id].size = new_size;
            for (size_t i = 0; i < metas_.size(); ++i) {
                metas_[i].offset = new_offsets[i];
            }
            arena_size_ = new_arena_size;
            return true;
        }

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
            std::unique_lock<std::mutex> lock(reconfig_mu_);
            while (reconfig_.load(std::memory_order_acquire)) {
                reconfig_cv_.wait(lock, [this]() { 
                    return !reconfig_.load(std::memory_order_acquire); 
                });
            }
            active_ops_.fetch_add(1, std::memory_order_acq_rel);
        }

        void op_exit() const { active_ops_.fetch_sub(1, std::memory_order_acq_rel); }

        void config_begin()
        {
            config_mu_.lock();
            /* 必须在持有 reconfig_mu_ 的情况下设置 reconfig_=true，
             * 否则与 op_enter() 之间存在 TOCTOU：
             *   op_enter 检查 reconfig_=false → config_begin 设置 true → op_enter 递增 active_ops
             * 此时 config_begin 等 active_ops==0 永远不会满足，死锁。
             * 持有 reconfig_mu_ 后，op_enter 要么在设置前完成递增，
             * 要么在设置后阻塞在 reconfig_cv_.wait，两种情况都安全。*/
            {
                std::lock_guard<std::mutex> lock(reconfig_mu_);
                reconfig_.store(true, std::memory_order_release);
            }
            while (active_ops_.load(std::memory_order_acquire))
                std::this_thread::yield();
        }

        void config_end()
        {
            {
                std::lock_guard<std::mutex> lock(reconfig_mu_);
                reconfig_.store(false, std::memory_order_release);
            }
            config_mu_.unlock();
            reconfig_cv_.notify_all();
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
