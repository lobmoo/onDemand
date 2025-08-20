// GovernanceManager.cpp
#include "GovernanceManager.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include "tinyxml2.h"
#include <dlfcn.h>

using namespace tinyxml2;

GovernanceManager::GovernanceManager()
{
    // 初始化 XML 内容
    m_doc.NewElement("dds");
    XMLElement *root = m_doc.NewElement("dds");
    m_doc.InsertFirstChild(root);

    root->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root->SetAttribute("xsi:noNamespaceSchemaLocation", "omg_shared_ca_domain_governance.xsd");

    XMLElement *domain_access_rules = m_doc.NewElement("domain_access_rules");
    root->InsertEndChild(domain_access_rules);
}

GovernanceManager *GovernanceManager::getInstance()
{
    static GovernanceManager *instance = new GovernanceManager; // 保证单例
    return instance;
}

bool GovernanceManager::addDomainRule(int min_id, int max_id,
                                      const DomainRuleAttributes &attributes)
{
    // 检查是否已经存在相同的 min 和 max 范围
    if (findDomainRule(min_id, max_id) != nullptr) {
        LOG(error) << "Domain rule with the given min and max already exists!" << std::endl;
        return false; // 如果已经存在则返回 false
    }

    // 创建一个新的 domain rule 元素并将其插入到 XML 中
    XMLElement *domainRuleElement = createDomainRuleElement(min_id, max_id, attributes);

    XMLElement *root = m_doc.FirstChildElement("dds");
    XMLElement *domain_access_rules = root->FirstChildElement("domain_access_rules");
    // 将新规则插入到 <domain_access_rules> 中
    domain_access_rules->InsertEndChild(domainRuleElement);

    return true;
}

XMLElement *GovernanceManager::createDomainRuleElement(int min_id, int max_id,
                                                       const DomainRuleAttributes &attributes)
{
    XMLElement *domainRule = m_doc.NewElement("domain_rule");

    // 创建 <domains> 元素并设置 id_range
    XMLElement *domains = m_doc.NewElement("domains");
    XMLElement *idRange = m_doc.NewElement("id_range");
    XMLElement *minElement = m_doc.NewElement("min");
    minElement->SetText(min_id);
    XMLElement *maxElement = m_doc.NewElement("max");
    maxElement->SetText(max_id);
    idRange->InsertEndChild(minElement);
    idRange->InsertEndChild(maxElement);
    domains->InsertEndChild(idRange);
    domainRule->InsertEndChild(domains);

    // 设置其他属性
    XMLElement *allowUnauth = m_doc.NewElement("allow_unauthenticated_participants");
    allowUnauth->SetText(attributes.allow_unauthenticated_participants);
    domainRule->InsertEndChild(allowUnauth);

    XMLElement *enableJoin = m_doc.NewElement("enable_join_access_control");
    enableJoin->SetText(attributes.enable_join_access_control);
    domainRule->InsertEndChild(enableJoin);

    XMLElement *discoveryProtection = m_doc.NewElement("discovery_protection_kind");
    discoveryProtection->SetText(attributes.discovery_protection_kind.c_str());
    domainRule->InsertEndChild(discoveryProtection);

    XMLElement *livelinessProtection = m_doc.NewElement("liveliness_protection_kind");
    livelinessProtection->SetText(attributes.liveliness_protection_kind.c_str());
    domainRule->InsertEndChild(livelinessProtection);

    XMLElement *rtpsProtection = m_doc.NewElement("rtps_protection_kind");
    rtpsProtection->SetText(attributes.rtps_protection_kind.c_str());
    domainRule->InsertEndChild(rtpsProtection);

    return domainRule;
}

XMLElement *GovernanceManager::findDomainRule(int min_id, int max_id) const
{
    XMLElement *root = const_cast<tinyxml2::XMLElement *>(m_doc.FirstChildElement("dds"));
    XMLElement *domain_access_rules = root->FirstChildElement("domain_access_rules");
    for (XMLElement *domainRule = domain_access_rules->FirstChildElement("domain_rule");
         domainRule != nullptr; domainRule = domainRule->NextSiblingElement("domain_rule")) {
        XMLElement *domains = domainRule->FirstChildElement("domains");
        XMLElement *idRange = domains->FirstChildElement("id_range");

        int min = idRange->FirstChildElement("min")->IntText();
        int max = idRange->FirstChildElement("max")->IntText();

        if (min == min_id && max == max_id) {
            return domainRule; // 找到对应的 domain rule
        }
    }
    return nullptr; // 没有找到对应的 domain rule
}

DomainRuleAttributes GovernanceManager::getDomainRuleAttributes(XMLElement *domainRule)
{
    DomainRuleAttributes attributes;

    // 从 domainRule 中读取现有的属性值，并填充到 DomainRuleAttributes 中
    XMLElement *allow_unauthenticated_participants =
        domainRule->FirstChildElement("allow_unauthenticated_participants");
    if (allow_unauthenticated_participants) {
        attributes.allow_unauthenticated_participants =
            allow_unauthenticated_participants->BoolText();
    }

    XMLElement *enable_join_access_control =
        domainRule->FirstChildElement("enable_join_access_control");
    if (enable_join_access_control) {
        attributes.enable_join_access_control = enable_join_access_control->BoolText();
    }

    XMLElement *discovery_protection_kind =
        domainRule->FirstChildElement("discovery_protection_kind");
    if (discovery_protection_kind) {
        attributes.discovery_protection_kind = discovery_protection_kind->GetText();
    }

    XMLElement *liveliness_protection_kind =
        domainRule->FirstChildElement("liveliness_protection_kind");
    if (liveliness_protection_kind) {
        attributes.liveliness_protection_kind = liveliness_protection_kind->GetText();
    }

    XMLElement *rtps_protection_kind = domainRule->FirstChildElement("rtps_protection_kind");
    if (rtps_protection_kind) {
        attributes.rtps_protection_kind = rtps_protection_kind->GetText();
    }

    return attributes;
}

void GovernanceManager::modifyDomainRule(XMLElement *domainRule,
                                         const DomainRuleAttributes &attributes)
{
    // 修改对应的属性值
    domainRule->FirstChildElement("allow_unauthenticated_participants")
        ->SetText(attributes.allow_unauthenticated_participants);
    domainRule->FirstChildElement("enable_join_access_control")
        ->SetText(attributes.enable_join_access_control);
    domainRule->FirstChildElement("discovery_protection_kind")
        ->SetText(attributes.discovery_protection_kind.c_str());
    domainRule->FirstChildElement("liveliness_protection_kind")
        ->SetText(attributes.liveliness_protection_kind.c_str());
    domainRule->FirstChildElement("rtps_protection_kind")
        ->SetText(attributes.rtps_protection_kind.c_str());
}

bool GovernanceManager::setAllowUnauthenticatedParticipants(int min_id, int max_id, bool value)
{
    XMLElement *domainRule = findDomainRule(min_id, max_id);
    if (domainRule == nullptr) {
        LOG(error) << "No domain rule found for the given min and max values!" << std::endl;
        return false;
    }

    DomainRuleAttributes attributes = getDomainRuleAttributes(domainRule);
    attributes.allow_unauthenticated_participants = value;
    modifyDomainRule(domainRule, attributes);
    return true;
}

bool GovernanceManager::setEnableJoinAccessControl(int min_id, int max_id, bool value)
{
    XMLElement *domainRule = findDomainRule(min_id, max_id);
    if (domainRule == nullptr) {
        LOG(error) << "No domain rule found for the given min and max values!";
        return false;
    }

    DomainRuleAttributes attributes = getDomainRuleAttributes(domainRule);
    attributes.enable_join_access_control = value;
    modifyDomainRule(domainRule, attributes);
    return true;
}

bool GovernanceManager::setDiscoveryProtectionKind(int min_id, int max_id, const std::string &value)
{
    XMLElement *domainRule = findDomainRule(min_id, max_id);
    if (domainRule == nullptr) {
        LOG(error) << "No domain rule found for the given min and max values!";
        return false;
    }

    DomainRuleAttributes attributes = getDomainRuleAttributes(domainRule);
    attributes.discovery_protection_kind = value;
    modifyDomainRule(domainRule, attributes);
    return true;
}

bool GovernanceManager::setLivelinessProtectionKind(int min_id, int max_id,
                                                    const std::string &value)
{
    XMLElement *domainRule = findDomainRule(min_id, max_id);
    if (domainRule == nullptr) {
        LOG(error) << "No domain rule found for the given min and max values!";
        return false;
    }

    DomainRuleAttributes attributes = getDomainRuleAttributes(domainRule);
    attributes.liveliness_protection_kind = value;
    modifyDomainRule(domainRule, attributes);
    return true;
}

bool GovernanceManager::setRtpsProtectionKind(int min_id, int max_id, const std::string &value)
{
    XMLElement *domainRule = findDomainRule(min_id, max_id);
    if (domainRule == nullptr) {
        LOG(error) << "No domain rule found for the given min and max values!";
        return false;
    }

    DomainRuleAttributes attributes = getDomainRuleAttributes(domainRule);
    attributes.rtps_protection_kind = value;
    modifyDomainRule(domainRule, attributes);
    return true;
}

std::string GovernanceManager::getXML() const
{
    XMLPrinter printer;
    m_doc.Accept(&printer);
    return printer.CStr(); // 返回格式化的 XML 字符串
}

bool GovernanceManager::addTopicRule(int min_id, int max_id, const std::string &topic_expression,
                                     const TopicRuleAttributes &topicRule)
{
    // 检查是否已经存在相同的 min 和 max 范围
    if (findDomainRule(min_id, max_id) == nullptr) {
        LOG(error) << "Domain rule with the given min and max not found!";
        return false; // 如果没有找到相应的 Domain Rule，返回 false
    }

    // 创建一个新的 topic rule 元素并将其插入到 XML 中
    XMLElement *topicRuleElement = createTopicRuleElement(topic_expression, topicRule);

    XMLElement *root = m_doc.FirstChildElement("dds");
    XMLElement *domain_access_rules = root->FirstChildElement("domain_access_rules");
    XMLElement *domainRuleElement = findDomainRule(min_id, max_id); // 获取对应的 domainRule
    XMLElement *topic_access_rules = domainRuleElement->FirstChildElement("topic_access_rules");
    if (!topic_access_rules) { // 不存在就新建一个
        topic_access_rules = m_doc.NewElement("topic_access_rules");
        domainRuleElement->InsertEndChild(topic_access_rules);
    }

    // 将新规则插入到 <topic_access_rules> 中
    topic_access_rules->InsertEndChild(topicRuleElement);

    return true;
}

XMLElement *GovernanceManager::createTopicRuleElement(const std::string &topic_expression,
                                                      const TopicRuleAttributes &topicRule)
{
    XMLElement *topicRuleElement = m_doc.NewElement("topic_rule");

    // 添加 topic_expression 元素
    XMLElement *topicExpressionElement = m_doc.NewElement("topic_expression");
    topicExpressionElement->SetText(topic_expression.c_str());
    topicRuleElement->InsertEndChild(topicExpressionElement);

    // 添加各个属性
    XMLElement *enableDiscoveryProtectionElement = m_doc.NewElement("enable_discovery_protection");
    enableDiscoveryProtectionElement->SetText(topicRule.enable_discovery_protection);
    topicRuleElement->InsertEndChild(enableDiscoveryProtectionElement);

    XMLElement *enableLivelinessProtectionElement =
        m_doc.NewElement("enable_liveliness_protection");
    enableLivelinessProtectionElement->SetText(topicRule.enable_liveliness_protection);
    topicRuleElement->InsertEndChild(enableLivelinessProtectionElement);

    XMLElement *enableReadAccessControlElement = m_doc.NewElement("enable_read_access_control");
    enableReadAccessControlElement->SetText(topicRule.enable_read_access_control);
    topicRuleElement->InsertEndChild(enableReadAccessControlElement);

    XMLElement *enableWriteAccessControlElement = m_doc.NewElement("enable_write_access_control");
    enableWriteAccessControlElement->SetText(topicRule.enable_write_access_control);
    topicRuleElement->InsertEndChild(enableWriteAccessControlElement);

    XMLElement *metadataProtectionKindElement = m_doc.NewElement("metadata_protection_kind");
    metadataProtectionKindElement->SetText(topicRule.metadata_protection_kind.c_str());
    topicRuleElement->InsertEndChild(metadataProtectionKindElement);

    XMLElement *dataProtectionKindElement = m_doc.NewElement("data_protection_kind");
    dataProtectionKindElement->SetText(topicRule.data_protection_kind.c_str());
    topicRuleElement->InsertEndChild(dataProtectionKindElement);

    return topicRuleElement;
}

XMLElement *GovernanceManager::findTopicRule(int min_id, int max_id,
                                             const std::string &topic_expression) const
{
    XMLElement *root = const_cast<tinyxml2::XMLElement *>(m_doc.FirstChildElement("dds"));
    XMLElement *domain_access_rules = root->FirstChildElement("domain_access_rules");
    for (XMLElement *domainRule = domain_access_rules->FirstChildElement("domain_rule");
         domainRule != nullptr; domainRule = domainRule->NextSiblingElement("domain_rule")) {
        XMLElement *domains = domainRule->FirstChildElement("domains");
        XMLElement *idRange = domains->FirstChildElement("id_range");

        int min = idRange->FirstChildElement("min")->IntText();
        int max = idRange->FirstChildElement("max")->IntText();

        if (min == min_id && max == max_id) {
            // 查找 topic_access_rules
            XMLElement *topic_access_rules = domainRule->FirstChildElement("topic_access_rules");
            if (topic_access_rules) {
                for (XMLElement *topicRuleElement =
                         topic_access_rules->FirstChildElement("topic_rule");
                     topicRuleElement != nullptr;
                     topicRuleElement = topicRuleElement->NextSiblingElement("topic_rule")) {
                    XMLElement *topic_expression_element =
                        topicRuleElement->FirstChildElement("topic_expression");
                    if (topic_expression_element
                        && topic_expression_element->GetText() == topic_expression) {
                        return topicRuleElement; // 找到对应的 topic_rule
                    }
                }
            }
        }
    }
    return nullptr; // 没有找到对应的 topic rule
}

TopicRuleAttributes GovernanceManager::getTopicRuleAttributes(XMLElement *topicRule)
{
    TopicRuleAttributes attributes;

    XMLElement *enable_discovery_protection =
        topicRule->FirstChildElement("enable_discovery_protection");
    if (enable_discovery_protection) {
        attributes.enable_discovery_protection = enable_discovery_protection->BoolText();
    }

    XMLElement *enable_liveliness_protection =
        topicRule->FirstChildElement("enable_liveliness_protection");
    if (enable_liveliness_protection) {
        attributes.enable_liveliness_protection = enable_liveliness_protection->BoolText();
    }

    XMLElement *enable_read_access_control =
        topicRule->FirstChildElement("enable_read_access_control");
    if (enable_read_access_control) {
        attributes.enable_read_access_control = enable_read_access_control->BoolText();
    }

    XMLElement *enable_write_access_control =
        topicRule->FirstChildElement("enable_write_access_control");
    if (enable_write_access_control) {
        attributes.enable_write_access_control = enable_write_access_control->BoolText();
    }

    XMLElement *metadata_protection_kind = topicRule->FirstChildElement("metadata_protection_kind");
    if (metadata_protection_kind) {
        attributes.metadata_protection_kind = metadata_protection_kind->GetText();
    }

    XMLElement *data_protection_kind = topicRule->FirstChildElement("data_protection_kind");
    if (data_protection_kind) {
        attributes.data_protection_kind = data_protection_kind->GetText();
    }

    return attributes;
}

void GovernanceManager::modifyTopicRule(XMLElement *topicRule,
                                        const TopicRuleAttributes &attributes)
{
    topicRule->FirstChildElement("enable_discovery_protection")
        ->SetText(attributes.enable_discovery_protection);
    topicRule->FirstChildElement("enable_liveliness_protection")
        ->SetText(attributes.enable_liveliness_protection);
    topicRule->FirstChildElement("enable_read_access_control")
        ->SetText(attributes.enable_read_access_control);
    topicRule->FirstChildElement("enable_write_access_control")
        ->SetText(attributes.enable_write_access_control);
    topicRule->FirstChildElement("metadata_protection_kind")
        ->SetText(attributes.metadata_protection_kind.c_str());
    topicRule->FirstChildElement("data_protection_kind")
        ->SetText(attributes.data_protection_kind.c_str());
}

bool GovernanceManager::setEnableDiscoveryProtection(int min_id, int max_id,
                                                     const std::string &topic_expression,
                                                     bool value)
{
    XMLElement *topicRule = findTopicRule(min_id, max_id, topic_expression);
    if (topicRule == nullptr) {
        LOG(error) << "No topic rule found for the given min, max values and topic expression!";
        return false;
    }

    TopicRuleAttributes attributes = getTopicRuleAttributes(topicRule);
    attributes.enable_discovery_protection = value;
    modifyTopicRule(topicRule, attributes);
    return true;
}

bool GovernanceManager::setEnableLivelinessProtection(int min_id, int max_id,
                                                      const std::string &topic_expression,
                                                      bool value)
{
    XMLElement *topicRule = findTopicRule(min_id, max_id, topic_expression);
    if (topicRule == nullptr) {
        LOG(error) << "No topic rule found for the given min, max values and topic expression!";
        return false;
    }

    TopicRuleAttributes attributes = getTopicRuleAttributes(topicRule);
    attributes.enable_liveliness_protection = value;
    modifyTopicRule(topicRule, attributes);
    return true;
}

bool GovernanceManager::setEnableReadAccessControl(int min_id, int max_id,
                                                   const std::string &topic_expression, bool value)
{
    XMLElement *topicRule = findTopicRule(min_id, max_id, topic_expression);
    if (topicRule == nullptr) {
        LOG(error) << "No topic rule found for the given min, max values and topic expression!";
        return false;
    }

    TopicRuleAttributes attributes = getTopicRuleAttributes(topicRule);
    attributes.enable_read_access_control = value;
    modifyTopicRule(topicRule, attributes);
    return true;
}

bool GovernanceManager::setEnableWriteAccessControl(int min_id, int max_id,
                                                    const std::string &topic_expression, bool value)
{
    XMLElement *topicRule = findTopicRule(min_id, max_id, topic_expression);
    if (topicRule == nullptr) {
        LOG(error) << "No topic rule found for the given min, max values and topic expression!";
        return false;
    }

    TopicRuleAttributes attributes = getTopicRuleAttributes(topicRule);
    attributes.enable_write_access_control = value;
    modifyTopicRule(topicRule, attributes);
    return true;
}

bool GovernanceManager::setMetadataProtectionKind(int min_id, int max_id,
                                                  const std::string &topic_expression,
                                                  const std::string &value)
{
    XMLElement *topicRule = findTopicRule(min_id, max_id, topic_expression);
    if (topicRule == nullptr) {
        LOG(error) << "No topic rule found for the given min, max values and topic expression!";
        return false;
    }

    TopicRuleAttributes attributes = getTopicRuleAttributes(topicRule);
    attributes.metadata_protection_kind = value;
    modifyTopicRule(topicRule, attributes);
    return true;
}

bool GovernanceManager::setDataProtectionKind(int min_id, int max_id,
                                              const std::string &topic_expression,
                                              const std::string &value)
{
    XMLElement *topicRule = findTopicRule(min_id, max_id, topic_expression);
    if (topicRule == nullptr) {
        LOG(error) << "No topic rule found for the given min, max values and topic expression!";
        return false;
    }

    TopicRuleAttributes attributes = getTopicRuleAttributes(topicRule);
    attributes.data_protection_kind = value;
    modifyTopicRule(topicRule, attributes);
    return true;
}
bool GovernanceManager::saveToFile(std::string &filename)
{
    LOG(info) << "Saving File to: " << filename;
    return m_doc.SaveFile(filename.c_str()) == XML_SUCCESS; // 保存到文件
}

bool GovernanceManager::signGovernanceFile(const std::string &governance,
                                           const std::string &cert_file,
                                           const std::string &key_file, const std::string &out_file)
{
    // OpenSSL_add_all_algorithms();
    // ERR_load_crypto_strings();

    BIO *in = BIO_new_file(governance.c_str(), "r");
    BIO *out = BIO_new_file(out_file.c_str(), "w");
    BIO *cert_bio = BIO_new_file(cert_file.c_str(), "r");
    BIO *key_bio = BIO_new_file(key_file.c_str(), "r");

    if (!in || !out || !cert_bio || !key_bio) {
        LOG(error) << "Failed to open file(s).";
        return false;
    }

    // 读取证书
    X509 *signer_cert = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
    if (!signer_cert) {
        LOG(error) << "Failed to read signer certificate.";
        return false;
    }

    // 读取私钥
    EVP_PKEY *signer_key = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
    if (!signer_key) {
        LOG(error) << "Failed to read private key.";
        return false;
    }

    // 执行签名
    PKCS7 *p7 = PKCS7_sign(signer_cert, signer_key, nullptr, in, PKCS7_TEXT | PKCS7_DETACHED);
    if (!p7) {
        LOG(error) << "PKCS7_sign failed.";
        // ERR_print_errors_fp(stderr);
        return false;
    }

    // 重置输入流（因为上面读取过一次）
    BIO_free(in);
    in = BIO_new_file(governance.c_str(), "r");

    // 输出 S/MIME 文件
    if (!SMIME_write_PKCS7(out, p7, in, PKCS7_TEXT | PKCS7_DETACHED)) {
        LOG(error) << "Failed to write S/MIME file.";
        // ERR_print_errors_fp(stderr);
        return false;
    }

    LOG(info) << "Saving Signed File to: " << out_file;
    // 释放资源
    PKCS7_free(p7);
    X509_free(signer_cert);
    EVP_PKEY_free(signer_key);
    BIO_free(in);
    BIO_free(out);
    BIO_free(cert_bio);
    BIO_free(key_bio);

    return true;
}