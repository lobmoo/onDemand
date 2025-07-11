#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <cstring>
#include <algorithm>

namespace fifo
{

enum class ErrorCode : int {
    Success = 0,
    Timeout = 300,
    NoResources = 301,
    UserCreateFail = 302,
    NoSuchUser = 303,
    CondWaitFail = 304,
    CondTimedWaitFail = 305,
    HeadMagicMismatch = 401,
    HeadChksumMismatch = 402,
    UserBuffNotEnough = 403,
    InvalidInput = 404,
    SizeTooSmall = 405,
    OutOfMemory = 406,
    InvalidFifo = 407,
    BadFifo = 408,
    WriteLenTooBig = 409,
    InvalidFifoUser = 410,
    Peek = 411
};

// 将错误码转换为字符串描述
inline std::string error_to_string(ErrorCode err)
{
    static const std::unordered_map<ErrorCode, std::string> error_map = {
        {ErrorCode::Success, "Success"},
        {ErrorCode::Timeout, "Timeout"},
        {ErrorCode::NoResources, "NoResources"},
        {ErrorCode::UserCreateFail, "UserCreateFail"},
        {ErrorCode::NoSuchUser, "NoSuchUser"},
        {ErrorCode::CondWaitFail, "CondWaitFail"},
        {ErrorCode::CondTimedWaitFail, "CondTimedWaitFail"},
        {ErrorCode::HeadMagicMismatch, "HeadMagicMismatch"},
        {ErrorCode::HeadChksumMismatch, "HeadChksumMismatch"},
        {ErrorCode::UserBuffNotEnough, "UserBuffNotEnough"},
        {ErrorCode::InvalidInput, "InvalidInput"},
        {ErrorCode::SizeTooSmall, "SizeTooSmall"},
        {ErrorCode::OutOfMemory, "OutOfMemory"},
        {ErrorCode::InvalidFifo, "InvalidFifo"},
        {ErrorCode::BadFifo, "BadFifo"},
        {ErrorCode::WriteLenTooBig, "WriteLenTooBig"},
        {ErrorCode::InvalidFifoUser, "InvalidFifoUser"},
        {ErrorCode::Peek, "Peek"}};
    auto it = error_map.find(err);
    return it != error_map.end() ? it->second : "Unknown_" + std::to_string(static_cast<int>(err));
}

// 自定义异常类，携带错误码
class FifoException : public std::runtime_error
{
public:
    FifoException(ErrorCode code) : std::runtime_error(error_to_string(code)), code_(code) {}
    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

// FIFO消息头结构
struct Header {
    static constexpr uint32_t Magic = ('F' << 24) | ('I' << 16) | ('F' << 8) | 'O';
    uint32_t magic{Magic};
    int32_t cmd{0};
    int32_t arg1{0};
    uint32_t len{0};
    uint32_t chksum{0};

    // 计算校验和（不包括chksum字段）
    void calculate_checksum()
    {
        chksum = 0;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(this);
        for (size_t i = 0; i < offsetof(Header, chksum); ++i) {
            chksum += ptr[i];
        }
    }

    // 验证消息头完整性
    bool verify() const
    {
        if (magic != Magic)
            return false;
        uint32_t calc_chksum = 0;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(this);
        for (size_t i = 0; i < offsetof(Header, chksum); ++i) {
            calc_chksum += ptr[i];
        }
        return calc_chksum == chksum;
    }
};

// FIFO用户类，管理读位置和同步
class FifoUser
{
    friend class Fifo;

public:
    FifoUser(const FifoUser &) = delete;
    FifoUser &operator=(const FifoUser &) = delete;
    ~FifoUser() = default;

private:
    explicit FifoUser(uint32_t read_pos) : read_pos_(read_pos), read_pos_initial_(read_pos) {}

    // 静态工厂方法，用于创建FifoUser
    static std::shared_ptr<FifoUser> create(uint32_t read_pos)
    {
        return std::shared_ptr<FifoUser>(new FifoUser(read_pos));
    }

    std::mutex mutex_;
    std::condition_variable cond_;
    uint32_t read_pos_;
    uint32_t read_pos_initial_;
};

// FIFO队列主类
class Fifo
{
public:
    static constexpr size_t MinDataSize = 128;
    static constexpr size_t MinSize = sizeof(Header) + MinDataSize;
    static constexpr uint32_t InvalidIdx = 0xffffffff;

    struct Params {
        uint32_t size{MinSize};
    };

    // 构造函数，初始化缓冲区
    explicit Fifo(const Params &params) : params_(params)
    {
        if (params.size < MinSize) {
            throw FifoException(ErrorCode::SizeTooSmall);
        }
        try {
            buffer_ = std::make_unique<uint8_t[]>(params.size);
        } catch (const std::bad_alloc &) {
            throw FifoException(ErrorCode::OutOfMemory);
        }
    }

    // 写入数据到FIFO
    void write(int32_t cmd, int32_t arg1, const void *data, uint32_t len)
    {
        if (len > 0 && !data) {
            throw FifoException(ErrorCode::InvalidInput);
        }
        if (len + sizeof(Header) > params_.size) {
            throw FifoException(ErrorCode::WriteLenTooBig);
        }
        adjust_user_read_positions(len + sizeof(Header));
        Header head;
        head.cmd = cmd;
        head.arg1 = arg1;
        head.len = len;
        head.calculate_checksum();
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            write_to_buffer(&head, sizeof(head));
            if (data && len > 0) {
                write_to_buffer(data, len);
            }
            read_pos_ = write_pos_;
            write_pos_ = (write_pos_ + sizeof(head) + len) % params_.size;
        }
        notify_users();
    }

    // 添加新用户
    std::shared_ptr<FifoUser> add_user(bool start_from_beginning = false)
    {
        auto user = FifoUser::create(start_from_beginning ? read_pos_ : write_pos_);
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        users_.push_back(user);
        return user;
    }

    // 克隆现有用户
    std::shared_ptr<FifoUser> clone_user(const FifoUser *user)
    {
        if (!user)
            throw FifoException(ErrorCode::InvalidFifoUser);
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        if (std::none_of(users_.begin(), users_.end(), [user](const auto &wp) {
                return !wp.expired() && wp.lock().get() == user;
            })) {
            throw FifoException(ErrorCode::NoSuchUser);
        }
        auto clone = FifoUser::create(user->read_pos_);
        users_.push_back(clone);
        return clone;
    }

    // 读取数据
    size_t read(FifoUser *user, int32_t *cmd, int32_t *arg1, void *buffer, size_t size, bool peek,
                int wait_ms)
    {
        if (!user)
            throw FifoException(ErrorCode::InvalidFifoUser);
        std::unique_lock<std::mutex> lock(user->mutex_);
        if (write_pos_ == user->read_pos_) {
            if (wait_ms == 0)
                throw FifoException(ErrorCode::Timeout);
            if (wait_ms > 0) {
                if (!user->cond_.wait_for(lock, std::chrono::milliseconds(wait_ms),
                                          [this, user] { return write_pos_ != user->read_pos_; })) {
                    throw FifoException(ErrorCode::Timeout);
                }
            } else {
                user->cond_.wait(lock, [this, user] { return write_pos_ != user->read_pos_; });
            }
        }
        Header head;
        read_from_buffer(&head, user->read_pos_, sizeof(head));
        if (!head.verify()) {
            user->read_pos_ = read_pos_;
            user->read_pos_initial_ = read_pos_;
            throw FifoException(ErrorCode::HeadMagicMismatch);
        }
        uint32_t read_pos = user->read_pos_;
        if (buffer && size > 0) {
            if (head.len > size) {
                throw FifoException(ErrorCode::UserBuffNotEnough);
            }
            read_pos = (read_pos + sizeof(head)) % params_.size;
            read_from_buffer(buffer, read_pos, head.len);
        }
        if (cmd)
            *cmd = head.cmd;
        if (arg1)
            *arg1 = head.arg1;
        read_pos = (read_pos + sizeof(head) + head.len) % params_.size;
        if (!peek) {
            user->read_pos_ = read_pos;
            user->read_pos_initial_ = read_pos;
        }
        return head.len;
    }

    // 移除用户
    void remove_user(FifoUser *user)
    {
        if (!user)
            throw FifoException(ErrorCode::InvalidFifoUser);
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        users_.erase(std::remove_if(users_.begin(), users_.end(),
                                    [user](const auto &wp) {
                                        return wp.expired() || wp.lock().get() == user;
                                    }),
                     users_.end());
    }

    // 重置用户读位置
    void reset_user(FifoUser *user)
    {
        if (!user)
            throw FifoException(ErrorCode::InvalidFifoUser);
        std::lock_guard<std::mutex> lock(user->mutex_);
        user->read_pos_ = write_pos_;
        user->read_pos_initial_ = write_pos_;
    }

private:
    // 写入缓冲区
    void write_to_buffer(const void *src, size_t len)
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        size_t part1 = std::min(len, static_cast<size_t>(params_.size - write_pos_));
        if (part1 > 0) {
            std::memcpy(buffer_.get() + write_pos_, src, part1);
        }
        size_t part2 = len - part1;
        if (part2 > 0) {
            std::memcpy(buffer_.get(), static_cast<const uint8_t *>(src) + part1, part2);
        }
    }

    // 从缓冲区读取
    void read_from_buffer(void *dst, uint32_t pos, size_t len)
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        size_t part1 = std::min(len, static_cast<size_t>(params_.size - pos));
        if (part1 > 0) {
            std::memcpy(dst, buffer_.get() + pos, part1);
        }
        size_t part2 = len - part1;
        if (part2 > 0) {
            std::memcpy(static_cast<uint8_t *>(dst) + part1, buffer_.get(), part2);
        }
    }

    // 计算可用空间
    uint32_t get_free_length(uint32_t user_read_pos) const
    {
        return (user_read_pos + params_.size - write_pos_ - 1) % params_.size;
    }

    // 调整用户读位置以避免覆盖
    void adjust_user_read_positions(uint32_t len)
    {
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        for (auto &wp : users_) {
            if (auto user = wp.lock()) {
                std::lock_guard<std::mutex> user_lock(user->mutex_);
                if (get_free_length(user->read_pos_) < len) {
                    if (overwrite_pos_ == InvalidIdx) {
                        drop_user_data(user.get(), len);
                    } else {
                        user->read_pos_ = overwrite_pos_;
                        user->read_pos_initial_ = overwrite_pos_;
                    }
                }
            }
        }
        overwrite_pos_ = InvalidIdx;
    }

    // 丢弃用户数据以腾出空间
    void drop_user_data(FifoUser *user, uint32_t len)
    {
        uint32_t dropped = 0;
        while (dropped < len) {
            Header head;
            read_from_buffer(&head, user->read_pos_, sizeof(head));
            if (!head.verify()) {
                user->read_pos_ = read_pos_;
                user->read_pos_initial_ = read_pos_;
                break;
            }
            dropped += sizeof(head) + head.len;
            user->read_pos_ = (user->read_pos_ + sizeof(head) + head.len) % params_.size;
        }
        user->read_pos_initial_ = user->read_pos_;
        overwrite_pos_ = user->read_pos_;
    }

    // 通知有未读数据的用户
    void notify_users()
    {
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        for (auto &wp : users_) {
            if (auto user = wp.lock()) {
                std::lock_guard<std::mutex> user_lock(user->mutex_);
                if (write_pos_ != user->read_pos_) {
                    user->cond_.notify_one();
                }
            }
        }
    }

    Params params_;
    std::unique_ptr<uint8_t[]> buffer_;
    std::mutex buffer_mutex_;
    std::vector<std::weak_ptr<FifoUser>> users_;
    std::mutex user_list_mutex_;
    uint32_t read_pos_{0};
    uint32_t write_pos_{0};
    uint32_t overwrite_pos_{InvalidIdx};
};

} // namespace fifo