// SecurityManager.h

#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include "log/logger.h"
#include <tinyxml2.h>
#include <string>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>

// 定义 DomainRuleAttributes 结构体，带默认值
struct DomainRuleAttributes {
    bool allow_unauthenticated_participants;
    bool enable_join_access_control;
    std::string discovery_protection_kind;
    std::string liveliness_protection_kind;
    std::string rtps_protection_kind;

    // 默认构造函数，设置默认值
    DomainRuleAttributes(bool allow_unauthenticated_participants = true,
                         bool enable_join_access_control = false,
                         std::string discovery_protection_kind = "NONE",
                         std::string liveliness_protection_kind = "NONE",
                         std::string rtps_protection_kind = "NONE")
        : allow_unauthenticated_participants(allow_unauthenticated_participants),
          enable_join_access_control(enable_join_access_control),
          discovery_protection_kind(discovery_protection_kind),
          liveliness_protection_kind(liveliness_protection_kind),
          rtps_protection_kind(rtps_protection_kind)
    {
    }
    void print()
    {
        LOG(info) << "allow_unauthenticated_participants: " << allow_unauthenticated_participants;
        LOG(info) << "enable_join_access_control: " << enable_join_access_control;
        LOG(info) << "discovery_protection_kind: " << discovery_protection_kind;
        LOG(info) << "liveliness_protection_kind: " << liveliness_protection_kind;
        LOG(info) << "rtps_protection_kind: " << rtps_protection_kind;
    }
};

struct TopicRuleAttributes {
    std::string topic_expression;         // 主题表达式
    bool enable_discovery_protection;     // 启用发现保护
    bool enable_liveliness_protection;    // 启用存活性保护
    bool enable_read_access_control;      // 启用读取访问控制
    bool enable_write_access_control;     // 启用写入访问控制
    std::string metadata_protection_kind; // 元数据保护类型
    std::string data_protection_kind;     // 数据保护类型

    // 构造函数：用于初始化 TopicRuleAttributes 的各个属性
    TopicRuleAttributes(const std::string &topic_expression = "",
                        bool enable_discovery_protection = false,
                        bool enable_liveliness_protection = false,
                        bool enable_read_access_control = false,
                        bool enable_write_access_control = false,
                        const std::string &metadata_protection_kind = "NONE",
                        const std::string &data_protection_kind = "NONE")
        : topic_expression(topic_expression),
          enable_discovery_protection(enable_discovery_protection),
          enable_liveliness_protection(enable_liveliness_protection),
          enable_read_access_control(enable_read_access_control),
          enable_write_access_control(enable_write_access_control),
          metadata_protection_kind(metadata_protection_kind),
          data_protection_kind(data_protection_kind)
    {
    }
};

class SecurityManager
{
public:
    // 获取单例实例
    static SecurityManager *getInstance();

    // 添加一个新的 domain rule
    bool addDomainRule(int min_id, int max_id, const DomainRuleAttributes &attributes);

    // 根据 min 和 max 修改 allow_unauthenticated_participants 属性
    bool setAllowUnauthenticatedParticipants(int min_id, int max_id, bool value);

    // 根据 min 和 max 修改 enable_join_access_control 属性
    bool setEnableJoinAccessControl(int min_id, int max_id, bool value);

    // 根据 min 和 max 修改 discovery_protection_kind 属性
    bool setDiscoveryProtectionKind(int min_id, int max_id, const std::string &value);

    // 根据 min 和 max 修改 liveliness_protection_kind 属性
    bool setLivelinessProtectionKind(int min_id, int max_id, const std::string &value);

    // 根据 min 和 max 修改 rtps_protection_kind 属性
    bool setRtpsProtectionKind(int min_id, int max_id, const std::string &value);

    bool addTopicRule(int min_id, int max_id, const std::string &topic_expression,
                      const TopicRuleAttributes &topicRule);

    // 返回当前的 XML 内容
    std::string getXML() const;

    // 将当前的 XML 保存到文件
    bool saveToFile(std::string &filename);

    // 单独修改某个属性
    bool setEnableDiscoveryProtection(int min_id, int max_id, const std::string &topic_expression,
                                      bool value);
    bool setEnableLivelinessProtection(int min_id, int max_id, const std::string &topic_expression,
                                       bool value);
    bool setEnableReadAccessControl(int min_id, int max_id, const std::string &topic_expression,
                                    bool value);
    bool setEnableWriteAccessControl(int min_id, int max_id, const std::string &topic_expression,
                                     bool value);
    bool setMetadataProtectionKind(int min_id, int max_id, const std::string &topic_expression,
                                   const std::string &value);
    bool setDataProtectionKind(int min_id, int max_id, const std::string &topic_expression,
                               const std::string &value);

    // 对governance.xml文件进行签名，得到smime文件
    bool signGovernanceFile(const std::string &governance, const std::string &cert_file,
                            const std::string &key_file, const std::string &out_file);

private:
    SecurityManager(); // 私有构造函数，禁止外部创建实例
    ~SecurityManager() = default;

    SecurityManager(const SecurityManager &) = delete;
    SecurityManager &operator=(const SecurityManager &) = delete;

    tinyxml2::XMLDocument m_doc; // 用来存储 XML 内容

    // 创建并返回一个新的 <domain_rule> 元素
    tinyxml2::XMLElement *createDomainRuleElement(int min_id, int max_id,
                                                  const DomainRuleAttributes &attributes);

    // 查找并返回对应 min 和 max 范围的 <domain_rule> 元素
    tinyxml2::XMLElement *findDomainRule(int min_id, int max_id) const;

    // 修改指定的 domain rule
    void modifyDomainRule(tinyxml2::XMLElement *domainRule, const DomainRuleAttributes &attributes);

    // 获取 Domain Rule 属性
    DomainRuleAttributes getDomainRuleAttributes(tinyxml2::XMLElement *domainRule);

    tinyxml2::XMLElement *createTopicRuleElement(const std::string &topic_expression,
                                                 const TopicRuleAttributes &topicRule);

    // 查找 Topic Rule
    tinyxml2::XMLElement *findTopicRule(int min_id, int max_id,
                                        const std::string &topic_expression) const;

    // 获取 Topic Rule 属性
    TopicRuleAttributes getTopicRuleAttributes(tinyxml2::XMLElement *topicRule);

    // 修改 Topic Rule 属性
    void modifyTopicRule(tinyxml2::XMLElement *topicRule, const TopicRuleAttributes &attributes);
};

#endif // SECURITY_MANAGER_H