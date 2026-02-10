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
        uint32_t id, offset, size;
    };

    struct alignas(64) Slot {
        std::atomic<uint32_t> seq{0};
        std::byte data[];
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
            if (new_count <= var_count_)
                return true;

            arena_size_ = align64(arena_size_);
            uint32_t old_size = var_count_
                                    ? metas_[var_count_ - 1].offset
                                          + align64(sizeof(Slot) + metas_[var_count_ - 1].size)
                                    : 0;

            void *new_arena = std::aligned_alloc(64, arena_size_);
            if (!new_arena)
                return false;
            if (old_size)
                std::memcpy(new_arena, arena_, old_size);
            if (arena_size_ > old_size)
                std::memset((std::byte *)new_arena + old_size, 0, arena_size_ - old_size);
            std::free(arena_);
            arena_ = (std::byte *)new_arena;

            auto *new_flags = new (std::nothrow) std::atomic<bool>[new_count];
            if (!new_flags)
                return false;
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

        bool write(uint32_t id, const void *src)
        {
            OpGuard g(this);
            if (!arena_ || !dirty_flags_ || id >= var_count_)
                return false;
            auto *slot = (Slot *)(arena_ + metas_[id].offset);

            slot->seq.fetch_add(1, std::memory_order_acquire);
            std::memcpy(slot->data, src, metas_[id].size);
            slot->seq.fetch_add(1, std::memory_order_release);

            if (!dirty_flags_[id].exchange(true, std::memory_order_acq_rel))
                dirty_queue_.enqueue(id);
            return true;
        }

        bool read(uint32_t id, void *dst) const
        {
            OpGuard g(this);
            if (!arena_ || id >= var_count_)
                return false;
            auto *slot = (const Slot *)(arena_ + metas_[id].offset);
            for (int retry = 0; retry < 10; ++retry) {
                uint32_t s1 = slot->seq.load(std::memory_order_acquire);
                if (s1 & 1)
                    continue;
                std::memcpy(dst, slot->data, metas_[id].size);
                if (s1 == slot->seq.load(std::memory_order_acquire))
                    return true;
            }
            return false;
        }

        class ScopedPtr
        {
            const VarStore *s_;
            const void *ptr_;

        public:
            ScopedPtr(const VarStore *s, uint32_t id) : s_(s), ptr_(nullptr)
            {
                if (!s_)
                    return;
                s_->op_enter();
                if (s_->arena_ && id < s_->var_count_) {
                    auto *slot = (const Slot *)(s_->arena_ + s_->metas_[id].offset);
                    ptr_ = slot->data;
                }
            }
            ~ScopedPtr()
            {
                if (s_)
                    s_->op_exit();
            }
            ScopedPtr(ScopedPtr &&o) noexcept : s_(o.s_), ptr_(o.ptr_) { o.s_ = nullptr; }
            ScopedPtr(const ScopedPtr &) = delete;
            const void *get() const { return ptr_; }
            explicit operator bool() const { return ptr_; }
        };

        ScopedPtr read_ptr(uint32_t id) const { return {this, id}; }

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