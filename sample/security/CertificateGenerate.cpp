#include "CertificateGenerate.h"
#include "log/logger.h"

CertificateGenerator::CertificateGenerator(const std::string& key_path,
                                           const std::string& cert_path,
                                           int validity_days)
    : key_path_(key_path),
      cert_path_(cert_path),
      validity_days_(validity_days),
      mainca_private_key_(nullptr),
      mainca_cert_(nullptr) {
    OpenSSL_add_all_algorithms();  // 初始化 OpenSSL 算法
}

bool CertificateGenerator::file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

bool CertificateGenerator::load_ca_from_file() {
    FILE* key_file = fopen(key_path_.c_str(), "r");
    if (!key_file) {
        std::cerr << "[ERROR] Cannot open CA private key file: " << key_path_ << std::endl;
        return false;
    }
    mainca_private_key_ = PEM_read_PrivateKey(key_file, nullptr, nullptr, nullptr);
    fclose(key_file);

    FILE* cert_file = fopen(cert_path_.c_str(), "r");
    if (!cert_file) {
        std::cerr << "[ERROR] Cannot open CA cert file: " << cert_path_ << std::endl;
        return false;
    }
    mainca_cert_ = PEM_read_X509(cert_file, nullptr, nullptr, nullptr);
    fclose(cert_file);

    if (!mainca_private_key_ || !mainca_cert_) {
        std::cerr << "[ERROR] Failed to load CA certificate or private key" << std::endl;
        return false;
    }

    std::cout << "[INFO] Successfully loaded existing CA key and certificate." << std::endl;
    return true;
}
// 根据传入的参数来生成自签名的证书-私钥对; 这个更简单，不需要配置文件
bool CertificateGenerator::generate_selfsigned_ca(const std::string& country, const std::string& stateOrProvinceName,
                                                   const std::string& localityName, const std::string& organization,
                                                   const std::string& common_name, const std::string& emailAddress) {
    /* 1.先检查是否已有证书和密钥*/
    if (file_exists(cert_path_) && file_exists(key_path_)) {
        std::cout << "[INFO] CA cert and key already exist. Loading..." << std::endl;
        return load_ca_from_file();
    }

    /* 2. 创建 EC 密钥对（椭圆曲线密钥） */
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec_key || !EC_KEY_generate_key(ec_key)) {
        std::cerr << "[ERROR] Failed to generate EC key." << std::endl;
        return false;
    }
 
    /* 3.将 EC 密钥绑定到 EVP_PKEY */ 
    EVP_PKEY* pkey = EVP_PKEY_new();
    EVP_PKEY_assign_EC_KEY(pkey, ec_key);

    /* 4.初始化 X.509 证书 */
    X509* cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), validity_days_ * 24 * 60 * 60L);
    X509_set_pubkey(cert, pkey);

    /* 5.添加 Subject 信息（国家、组织、域名等） */
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (const unsigned char*)country.c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (const unsigned char*)stateOrProvinceName.c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "L",  MBSTRING_ASC, (const unsigned char*)localityName.c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (const unsigned char*)organization.c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)common_name.c_str(), -1, -1, 0);
    
    X509_set_issuer_name(cert, name);  // 自签名

    if (!X509_sign(cert, pkey, EVP_sha256())) {
        std::cerr << "[ERROR] Failed to sign the self-signed certificate." << std::endl;
        return false;
    }

    return write_key_and_cert(pkey, cert);
}

bool CertificateGenerator::generate_selfsigned_ca_from_config(const std::string& config_file) {
    /* 1.判断是否已存在 */
    if (file_exists(cert_path_) && file_exists(key_path_)) {
        LOG(warning) << "CA cert and key already exist. Loading..." ;
        return load_ca_from_file();
    }
    /* 2.从路径中加载conf配置文件 */
    CONF* conf = NCONF_new(nullptr);
    if (NCONF_load(conf, config_file.c_str(), nullptr) <= 0) {
        std::cerr << "[ERROR] Failed to load config file: " << config_file << std::endl;
        return false;
    }
    /* 3.读取配置文件中的配置端名 */
    std::string dn_section = NCONF_get_string(conf, "req", "distinguished_name");  // 找req段中的dis...字段，来明确主机的subject name
    std::string ext_section = "v3_ca";  // 默认扩展字段段名为 "v3_ca"，用于 X.509 扩展

    /* 4.生成椭圆曲线密钥（ECC 密钥） */
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);  // EC_KEY_new_by_curve_name 选择椭圆曲线（这是 NIST P-256）
    EC_KEY_generate_key(ec_key);  //  生成私钥和公钥

    /* 5.将 EC 密钥绑定到 EVP_PKEY */ 
    EVP_PKEY* pkey = EVP_PKEY_new();  // OpenSSL 统一密钥接口是 EVP_PKEY，这里把 EC_KEY 包装进去 为后续生成 X.509 证书使用
    EVP_PKEY_assign_EC_KEY(pkey, ec_key);

    /* 6.初始化 X.509 证书 */
    X509* cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), validity_days_ * 24 * 60 * 60L);  // 设置起始时间为当前时间，有效期为类成员 validity_days_
    X509_set_pubkey(cert, pkey);  // 将 pkey 设置为公钥

    /* 7.添加 Subject 信息（国家、组织、域名等） */
    X509_NAME* name = X509_get_subject_name(cert);
    STACK_OF(CONF_VALUE)* dn = NCONF_get_section(conf, dn_section.c_str());
    for (int i = 0; i < sk_CONF_VALUE_num(dn); ++i) {
        CONF_VALUE* val = sk_CONF_VALUE_value(dn, i);
        X509_NAME_add_entry_by_txt(name, val->name, MBSTRING_ASC,
                                   (const unsigned char*)val->value, -1, -1, 0);
    }
    X509_set_issuer_name(cert, name);

    /* 8.添加扩展字段（如 basicConstraints=CA:TRUE） */
    X509V3_CTX ctx;
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
    STACK_OF(CONF_VALUE)* ext_list = NCONF_get_section(conf, ext_section.c_str());
    for (int i = 0; i < sk_CONF_VALUE_num(ext_list); ++i) {
        CONF_VALUE* val = sk_CONF_VALUE_value(ext_list, i);
        X509_EXTENSION* ext = X509V3_EXT_nconf(conf, &ctx, val->name, val->value);
        if (!ext) continue;
        X509_add_ext(cert, ext, -1);
        X509_EXTENSION_free(ext);
    }

    /* 9.用私钥对证书签名 */ 
    if (!X509_sign(cert, pkey, EVP_sha256())) {  // 使用 SHA256 和私钥对证书签名 这是自签名的关键步骤
        LOG(error) << "Failed to sign certificate from config.";
        return false;
    }

    return write_key_and_cert(pkey, cert);  // 将自签名后的私钥和证书写回磁盘
}

bool CertificateGenerator::write_key_and_cert(EVP_PKEY* pkey, X509* cert) {
    /* 1.将私钥写入文件 */
    FILE* key_file = fopen(key_path_.c_str(), "w");
    if (!key_file || !PEM_write_PrivateKey(key_file, pkey, nullptr, nullptr, 0, nullptr, nullptr)) {
        LOG(error) << "Failed to write private key to " << key_path_;
        return false;
    }
    fclose(key_file);
    /* 2.将证书写入文件 */
    FILE* cert_file = fopen(cert_path_.c_str(), "w");
    if (!cert_file || !PEM_write_X509(cert_file, cert)) {
        LOG(error) << "Failed to write certificate to " << cert_path_;
        return false;
    }
    fclose(cert_file);
    
    /* 3.将私钥和证书保存到类的内存成员中 */
    mainca_private_key_ = EVP_PKEY_dup(pkey);  // 赋值给类成员变量以便后续签名使用 在openssl-3.0版本中添加
    mainca_cert_ = X509_dup(cert);

    X509_free(cert);
    EVP_PKEY_free(pkey);
    return true;
}