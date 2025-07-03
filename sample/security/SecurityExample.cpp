// // CustomSecurityExample.cpp
// // 示例：基于官方PKI-DH认证，结合自定义访问控制插件，实现identity_handle与GUID映射及权限管理

// // #include <fastdds/rtps/security/accesscontrol/AccessControlPlugin.h>
// // #include <fastdds/rtps/security/authentication/AuthenticationPlugin.h>
// // #include <fastdds/rtps/security/authentication/PKIDHAuthenticationPlugin.h>
// #include <fastdds/dds/domain/DomainParticipantFactory.hpp>
// #include <fastdds/dds/domain/DomainParticipant.hpp>
// #include <fastdds/dds/publisher/Publisher.hpp>
// #include <fastdds/dds/subscriber/Subscriber.hpp>
// #include <fastdds/dds/topic/Topic.hpp>
// #include <fastdds/dds/publisher/DataWriter.hpp>
// #include <fastdds/dds/subscriber/DataReader.hpp>
// // #include <fastdds/dds/core/policy/PropertyQosPolicy.hpp>
// #include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
// #include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
// #include <fastdds/rtps/common/Guid.hpp>

// #include <iostream>
// #include <mutex>
// #include <map>
// #include <sstream>
// #include <iomanip>
// using namespace eprosima::fastdds::dds;
// using namespace eprosima::fastdds::rtps::security;
// using namespace eprosima::fastdds::rtps;

// // 权限定义
// enum class Permission { NONE = 0, READ = 1, WRITE = 2, READ_WRITE = 3 };

// struct TopicPermission {
//     Permission permission;
//     bool encrypt;
// };

// // 权限管理器，维护identity_handle与GUID映射，管理权限
// class SecurityManager {
// public:
//     void register_participant(void* identity_handle, const GUID_t& guid) {
//         std::lock_guard<std::mutex> lock(mutex_);
//         identity_to_guid_[identity_handle] = guid;
//         std::string guid_str = guid_to_str(guid);
//         guid_to_str_map_[guid] = guid_str;
//         std::cout << "[SecurityManager] Register identity_handle " << identity_handle
//                   << " GUID " << guid_str << std::endl;
//     }

//     void set_permission_by_guid(const GUID_t& guid,
//                                 const std::string& topic,
//                                 Permission perm,
//                                 bool encrypt) {
//         std::lock_guard<std::mutex> lock(mutex_);
//         std::string key = guid_to_str(guid);
//         permissions_[key][topic] = {perm, encrypt};
//         std::cout << "[SecurityManager] Set permission for GUID " << key
//                   << " topic " << topic
//                   << " perm " << static_cast<int>(perm)
//                   << " encrypt " << encrypt << std::endl;
//     }

//     bool check_permission(void* identity_handle, const std::string& topic, Permission required) {
//         std::lock_guard<std::mutex> lock(mutex_);
//         auto it = identity_to_guid_.find(identity_handle);
//         if (it == identity_to_guid_.end()) return false;
//         std::string key = guid_to_str(it->second);
//         auto perm_it = permissions_.find(key);
//         if (perm_it == permissions_.end()) return false;
//         auto topic_it = perm_it->second.find(topic);
//         if (topic_it == perm_it->second.end()) return false;
//         Permission p = topic_it->second.permission;
//         return (static_cast<int>(p) & static_cast<int>(required)) == static_cast<int>(required);
//     }

//     static std::string guid_to_str(const GUID_t& guid) {
//         std::ostringstream oss;
//         oss.fill('0');
//         oss << std::hex;
//         for (auto b : guid.guidPrefix.value) oss << std::setw(2) << (int)b;
//         for (auto b : guid.entityId.value) oss << std::setw(2) << (int)b;
//         return oss.str();
//     }

// private:
//     std::mutex mutex_;
//     std::map<void*, GUID_t> identity_to_guid_;
//     std::map<GUID_t, std::string> guid_to_str_map_;
//     std::map<std::string, std::map<std::string, TopicPermission>> permissions_;
// };

// // 自定义访问控制插件
// class MyAccessControlPlugin : public eprosima::fastdds::rtps::AccessControlPlugin {  
// public:
//     MyAccessControlPlugin(SecurityManager* manager) : manager_(manager) {}

//     bool check_create_datawriter(  //创建datawriter的时候自动判断是否有权限创建
//         void* identity_handle,
//         void* /*permissions_handle*/,
//         const std::string& topic_name,
//         const void* /*writer_attributes*/,
//         bool& /*secure_writer*/) override
//     {
//         bool allow = manager_->check_permission(identity_handle, topic_name, Permission::WRITE);
//         std::cout << "[AccessControl] check_create_datawriter topic=" << topic_name
//                   << " identity_handle=" << identity_handle
//                   << " allow=" << allow << std::endl;
//         return allow;
//     }

//     bool check_create_datareader(  //创建datareader的时候自动判断是否有权限创建
//         void* identity_handle,
//         void* /*permissions_handle*/,
//         const std::string& topic_name,
//         const void* /*reader_attributes*/,
//         bool& /*secure_reader*/) override
//     {
//         bool allow = manager_->check_permission(identity_handle, topic_name, Permission::READ);
//         std::cout << "[AccessControl] check_create_datareader topic=" << topic_name
//                   << " identity_handle=" << identity_handle
//                   << " allow=" << allow << std::endl;
//         return allow;
//     }

// private:
//     SecurityManager* manager_;
// };

// // 自定义身份认证插件，内部调用官方PKI-DH插件，注册映射
// class MyAuthenticationPlugin : public AuthenticationPlugin {
// public:
//     MyAuthenticationPlugin(SecurityManager* manager)
//         : manager_(manager), pki_plugin_(new PKIDHAuthenticationPlugin())
//     {}

//     ~MyAuthenticationPlugin() override {
//         delete pki_plugin_;
//     }

//     bool init(AuthenticationListener* listener, const PropertyPolicy& properties) override {
//         return pki_plugin_->init(listener, properties);
//     }

//     bool validate_local_identity(
//         void** identity_handle,
//         IdentityToken** identity_token,
//         const IdentityCredentialToken* identity_credential,
//         const GUID_t& participant_guid,
//         const PropertyPolicy& participant_properties) override
//     {
//         bool ok = pki_plugin_->validate_local_identity(identity_handle, identity_token,
//                                                        identity_credential,
//                                                        participant_guid,
//                                                        participant_properties);
//         if (!ok) return false;
//         manager_->register_participant(*identity_handle, participant_guid);
//         return true;
//     }

//     bool validate_remote_identity(
//         void** remote_identity_handle,
//         IdentityToken** remote_identity_token,
//         const IdentityToken* local_identity_token,
//         const GUID_t& remote_participant_guid,
//         const GUID_t& local_participant_guid,
//         const PropertyPolicy& participant_properties) override
//     {
//         return pki_plugin_->validate_remote_identity(remote_identity_handle,
//                                                      remote_identity_token,
//                                                      local_identity_token,
//                                                      remote_participant_guid,
//                                                      local_participant_guid,
//                                                      participant_properties);
//     }

//     bool return_identity_handle(void* identity_handle) override {
//         return pki_plugin_->return_identity_handle(identity_handle);
//     }

//     bool return_identity_token(IdentityToken* identity_token) override {
//         return pki_plugin_->return_identity_token(identity_token);
//     }

// private:
//     SecurityManager* manager_;
//     PKIDHAuthenticationPlugin* pki_plugin_;
// };

// // 主函数示例
// int main() {
//     SecurityManager* security_manager = new SecurityManager();

//     // 这里是要先注册一个participant，然后通过回调函数来得到他的GUID
//     GUID_t example_guid;
//     for (int i = 0; i < 12; ++i) example_guid.guidPrefix.value[i] = i + 1;
//     for (int i = 0; i < 4; ++i) example_guid.entityId.value[i] = i + 1;

//     security_manager->set_permission_by_guid(example_guid, "secure_topic", Permission::READ_WRITE, true);
//     security_manager->set_permission_by_guid(example_guid, "public_topic", Permission::READ, false);

//     MyAccessControlPlugin* access_plugin = new MyAccessControlPlugin(security_manager);
//     MyAuthenticationPlugin* auth_plugin = new MyAuthenticationPlugin(security_manager);

//     DomainParticipantQos qos;
//     qos.name("secure_participant");
//     qos.properties().properties.emplace_back("dds.sec.auth.plugin", "MyAuthenticationPlugin");
//     qos.properties().properties.emplace_back("dds.sec.access.plugin", "MyAccessControlPlugin");
//     // 使用默认内置加密插件
//     // qos.properties().properties.emplace_back("dds.sec.crypto.plugin", "builtin.AES-GCM");

//     DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, qos);
//     if (!participant) {
//         std::cerr << "Participant 创建失败" << std::endl;
//         return 1;
//     }

//     Topic* topic = participant->create_topic("secure_topic", "std::string", TOPIC_QOS_DEFAULT);
//     Publisher* pub = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
//     Subscriber* sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);

//     DataWriter* writer = pub->create_datawriter(topic, DATAWRITER_QOS_DEFAULT);
//     std::cout << "DataWriter 创建 " << (writer ? "成功" : "失败") << std::endl;

//     DataReader* reader = sub->create_datareader(topic, DATAREADER_QOS_DEFAULT);
//     std::cout << "DataReader 创建 " << (reader ? "成功" : "失败") << std::endl;

//     return 0;
// }