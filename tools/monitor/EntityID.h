#pragma once
#include <string>
#include <functional>

namespace Monitor
{

class EntityID
{
public:
    EntityID() = default;

    EntityID(std::string category, std::string id)
        : category_(std::move(category)), id_(std::move(id))
    {
    }

    // 拷贝/移动构造与赋值 = 默认
    EntityID(const EntityID &) = default;
    EntityID(EntityID &&) noexcept = default;
    EntityID &operator=(const EntityID &) = default;
    EntityID &operator=(EntityID &&) noexcept = default;

    // 比较运算符（==）
    bool operator==(const EntityID &other) const
    {
        return category_ == other.category_ && id_ == other.id_;
    }

    std::string toString() const { return "EntityID[category=" + category_ + ", id=" + id_ + "]"; }

  
    // 访问器
    const std::string &category() const { return category_; }
    const std::string &id() const { return id_; }

    // 可选：设置器（如果你希望对象是可修改的）
    void setCategory(const std::string &cat) { category_ = cat; }
    void setId(const std::string &identifier) { id_ = identifier; }

private:
    std::string category_;
    std::string id_;
};

  // ? 流输出支持：std::cout << eid;
    inline std::ostream &operator<<(std::ostream &os, const EntityID &eid)
    {
        return os << eid.toString();
    }

} // namespace Monitor

namespace std
{
template <>
struct hash<Monitor::EntityID> {
    std::size_t operator()(const Monitor::EntityID &eid) const noexcept
    {
        std::size_t h1 = std::hash<std::string>{}(eid.category());
        std::size_t h2 = std::hash<std::string>{}(eid.id());
        return h1 ^ (h2 << 1); // boost::hash_combine 风格
    }
};
} // namespace std