/*
 * @Author       : baihaoran 948942547@qq.com
 * @Date         : 2025-05-19 11:18:38
 * @FilePath     : /TXDDS/include/txdds/RTPS/dynamicTypeParser/BaseNode.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace BaoSky::rtps::DynamicBuilder
{
    enum class NodeType
    {
        PROFILES,
        PARTICIPANT,
        PUBLISHER,
        SUBSCRIBER,
        RTPS,
        QOS_PROFILE,
        APPLICATION,
        TYPE,
        TOPIC,
        DATA_WRITER,
        DATA_READER,
        ROOT,
        TYPES,
        LOG,
        REQUESTER,
        REPLIER,
        LIBRARY_SETTINGS
    };

    class BaseNode
    {
    public:
        virtual ~BaseNode() = default;
        BaseNode(const BaseNode &) = delete;
        BaseNode &operator=(const BaseNode &) = delete;
        BaseNode(BaseNode &&) = default;
        BaseNode &operator=(BaseNode &&) = default;
        BaseNode(NodeType type) : data_type_(type), parent_(nullptr)
        {
        }

        NodeType getType() const
        {
            return data_type_;
        }

        void addChild(std::unique_ptr<BaseNode> child)
        {
            child->setParent(this);
            children.push_back(std::move(child));
        }

        bool removeChild(const size_t &index)
        {
            auto valid = (children.size() > index);
            if (valid)
            {
                children.erase(children.begin() + index);
            }
            return valid;
        }
        BaseNode *getChild(const size_t &index) const
        {
            if (children.empty())
            {
                return nullptr;
            }
            return children[index].get();
        }

        BaseNode *getParent() const
        {
            return parent_;
        }

        void setParent(BaseNode *parent)
        {
            parent_ = parent;
        }

        size_t getNumChildren() const
        {
            return children.size();
        }

        std::vector<std::unique_ptr<BaseNode>> &getChildren()
        {
            return children;
        }

    private:
        NodeType data_type_;
        BaseNode *parent_;
        std::vector<std::unique_ptr<BaseNode>> children;
    };

    template <class T>
    class DataNode : public BaseNode
    {
    public:
        DataNode(const DataNode &) = delete;
        DataNode &operator=(const DataNode &) = delete;
        DataNode(DataNode &&) = default;
        DataNode &operator=(DataNode &&) = default;
        DataNode(NodeType type) : BaseNode(type), attributes_(), data_(nullptr)
        {
        }

        DataNode(NodeType type, std::unique_ptr<T> data) : BaseNode(type), attributes_(), data_(std::move(data))
        {
        }
        virtual ~DataNode()
        {
        }

        T *get() const
        {
            return data_.get();
        }

        std::unique_ptr<T> getData()
        {
            return std::move(data_);
        }

        void setData(std::unique_ptr<T> data)
        {
            data_ = std::move(data);
        }

        void addAttribute(const std::string &name, const std::string &value)
        {
            attributes_[name] = value;
        }

        const std::map<std::string, std::string> &getAttributes()
        {
            return attributes_;
        }

    private:
        std::map<std::string, std::string> attributes_;
        std::unique_ptr<T> data_;
    };

}
