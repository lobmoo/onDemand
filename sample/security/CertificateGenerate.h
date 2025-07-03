// #include <openssl/evp.h>
// #include <openssl/ec.h>
// #include <openssl/x509.h>
// #include <openssl/pem.h>
// #include <openssl/x509v3.h>
// #include <openssl/conf.h>
// #include <iostream>
// #include <fstream>
// #include <memory>

// class CertificateGenerator {  // 要搞成单例
// public:
//     CertificateGenerator(const std::string& key_path = "file:///home/weiqb/src/test_demo/config/certs2/maincakey.pem",
//                          const std::string& cert_path = "file:///home/weiqb/src/test_demo/config/certs2/maincacert.pem",
//                          int validity_days = 3650);
//     // 生成/加载中心ca证书&私钥的函数
//     bool generate_selfsigned_ca(const std::string& country, const std::string& stateOrProvinceName,
//                                 const std::string& localityName, const std::string& organization,
//                                 const std::string& common_name, const std::string& emailAddress);

//     bool generate_selfsigned_ca_from_config(const std::string& config_file);  // 从maincaconf.conf文件中加载

//     // 新增接口：从文件加载证书和密钥
//     bool load_ca_from_file();  // 直接从key_path和cert_path加载证书
    
// private:
//     std::string key_path_;
//     std::string cert_path_;
//     int validity_days_;

//     // 私有成员变量：加载后的CA信息
//     EVP_PKEY* mainca_private_key_ = nullptr;
//     X509* mainca_cert_ = nullptr;

//     bool write_key_and_cert(EVP_PKEY* pkey, X509* cert);
//     bool file_exists(const std::string& path);
// };