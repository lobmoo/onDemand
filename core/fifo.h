#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <array>

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

inline std::string error_to_string(ErrorCode err)
{
    static const std::array<std::pair<ErrorCode, const char *>, 12> error_map = {
        {{ErrorCode::Success, "Success"},
         {ErrorCode::Timeout, "Timeout"},
         {ErrorCode::NoResources, "NoResources"},
         {ErrorCode::UserCreateFail, "UserCreateFail"},
         {ErrorCode::NoSuchUser, "NoSuchUser"},
         {ErrorCode::HeadMagicMismatch, "HeadMagicMismatch"},
         {ErrorCode::HeadChksumMismatch, "HeadChksumMismatch"},
         {ErrorCode::UserBuffNotEnough, "UserBuffNotEnough"},
         {ErrorCode::InvalidInput, "InvalidInput"},
         {ErrorCode::SizeTooSmall, "SizeTooSmall"},
         {ErrorCode::OutOfMemory, "OutOfMemory"},
         {ErrorCode::InvalidFifo, "InvalidFifo"}}};
    auto it = std::find_if(error_map.begin(), error_map.end(),
                           [err](const auto &p) { return p.first == err; });
    if (it != error_map.end())
        return it->second;
    return "Unknown_" + std::to_string(static_cast<int>(err));
}

class FifoException : public std::runtime_error
{
public:
    FifoException(ErrorCode code) : std::runtime_error(error_to_string(code)), code_(code) {}
    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

struct Header {
    static constexpr uint32_t Magic = ('F' << 24) | ('I' << 16) | ('F' << 8) | 'O';
    uint32_t magic{Magic};
    int32_t cmd{0};
    int32_t arg1{0};
    uint32_t len{0};
    uint32_t chksum{0};

    void calculate_checksum()
    {
        chksum = 0;
        auto *ptr = reinterpret_cast<uint8_t *>(this);
        for (size_t i = 0; i < offsetof(Header, chksum); ++i) {
            chksum += ptr[i];
        }
    }

    bool verify() const
    {
        if (magic != Magic)
            return false;
        uint32_t calc_chksum = 0;
        auto *ptr = reinterpret_cast<const uint8_t *>(this);
        for (size_t i = 0; i < offsetof(Header, chksum); ++i) {
            calc_chksum += ptr[i];
        }
        return calc_chksum == chksum;
    }
};

class FifoUser
{
    friend class Fifo;

public:
    FifoUser(const FifoUser &) = delete;
    FifoUser &operator=(const FifoUser &) = delete;
    ~FifoUser() = default;

    static std::unique_ptr<FifoUser> create(uint32_t read_pos)
    {
        return std::unique_ptr<FifoUser>(new FifoUser(read_pos));
    }

private:
    FifoUser(uint32_t read_pos) : read_pos_(read_pos), read_pos_initial_(read_pos) {}
    std::mutex mutex_;
    std::condition_variable cond_;
    uint32_t read_pos_;
    uint32_t read_pos_initial_;
};

class Fifo
{
public:
    static constexpr size_t MinDataSize = 128;
    static constexpr size_t MinSize = sizeof(Header) + MinDataSize;
    static constexpr uint32_t InvalidIdx = 0xffffffff;

    struct Params {
        uint32_t size{MinSize};
    };

    explicit Fifo(const Params &params)
    {
        if (params.size < MinSize) {
            throw FifoException(ErrorCode::SizeTooSmall);
        }
        buffer_ = std::make_unique<uint8_t[]>(params.size);
        params_ = params;
    }

    void write(int32_t cmd, int32_t arg1, const void *data, uint32_t len)
    {
        if (len + sizeof(Header) >= params_.size) {
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

    std::unique_ptr<FifoUser> add_user(bool start_from_beginning = false)
    {
        auto user = FifoUser::create(start_from_beginning ? read_pos_ : write_pos_);
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        users_.push_back(user.get());
        return user;
    }

    std::unique_ptr<FifoUser> clone_user(const FifoUser *user)
    {
        if (!user)
            throw FifoException(ErrorCode::InvalidFifoUser);
        auto clone = FifoUser::create(user->read_pos_);
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        users_.push_back(clone.get());
        return clone;
    }

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
            user->read_pos_initial_ = user->read_pos_;
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

    void remove_user(FifoUser *user)
    {
        if (!user)
            throw FifoException(ErrorCode::InvalidFifoUser);
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        users_.erase(std::remove(users_.begin(), users_.end(), user), users_.end());
    }

    void reset_user(FifoUser *user)
    {
        if (!user)
            throw FifoException(ErrorCode::InvalidFifoUser);
        std::lock_guard<std::mutex> lock(user->mutex_);
        user->read_pos_ = write_pos_;
        user->read_pos_initial_ = write_pos_;
    }

private:
    void write_to_buffer(const void *src, size_t len)
    {
        size_t part1 = std::min(len, static_cast<size_t>(params_.size - write_pos_));
        if (part1 > 0) {
            std::memcpy(buffer_.get() + write_pos_, src, part1);
        }
        size_t part2 = len - part1;
        if (part2 > 0) {
            std::memcpy(buffer_.get(), static_cast<const uint8_t *>(src) + part1, part2);
        }
    }

    void read_from_buffer(void *dst, uint32_t pos, size_t len)
    {
        size_t part1 = std::min(len, static_cast<size_t>(params_.size - pos));
        if (part1 > 0) {
            std::memcpy(dst, buffer_.get() + pos, part1);
        }
        size_t part2 = len - part1;
        if (part2 > 0) {
            std::memcpy(static_cast<uint8_t *>(dst) + part1, buffer_.get(), part2);
        }
    }

    uint32_t get_free_length(uint32_t user_read_pos) const
    {
        return (user_read_pos + params_.size - write_pos_ - 1) % params_.size;
    }

    void adjust_user_read_positions(uint32_t len)
    {
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        for (auto *user : users_) {
            std::lock_guard<std::mutex> user_lock(user->mutex_);
            if (get_free_length(user->read_pos_) < len) {
                if (overwrite_pos_ == InvalidIdx) {
                    drop_user_data(user, len);
                } else {
                    user->read_pos_ = overwrite_pos_;
                    user->read_pos_initial_ = overwrite_pos_;
                }
            }
        }
        overwrite_pos_ = InvalidIdx;
    }

    void drop_user_data(FifoUser *user, uint32_t len)
    {
        uint32_t dropped = 0;
        while (dropped < len) {
            Header head;
            read_from_buffer(&head, user->read_pos_, sizeof(head));
            if (!head.verify()) {
                user->read_pos_ = read_pos_;
                user->read_pos_initial_ = user->read_pos_;
                break;
            }
            dropped += sizeof(head) + head.len;
            user->read_pos_ = (user->read_pos_ + sizeof(head) + head.len) % params_.size;
        }
        user->read_pos_initial_ = user->read_pos_;
        overwrite_pos_ = user->read_pos_;
    }

    void notify_users()
    {
        std::lock_guard<std::mutex> lock(user_list_mutex_);
        for (auto *user : users_) {
            std::lock_guard<std::mutex> user_lock(user->mutex_);
            user->cond_.notify_one();
        }
    }

    Params params_;
    std::unique_ptr<uint8_t[]> buffer_;
    std::mutex buffer_mutex_;
    std::vector<FifoUser *> users_;
    std::mutex user_list_mutex_;
    uint32_t read_pos_{0};
    uint32_t write_pos_{0};
    uint32_t overwrite_pos_{InvalidIdx};
};

} // namespace fifo