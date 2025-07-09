#!/bin/bash
set -e  # 一旦出错就退出脚本
# cd /home/weiqb/src/test_demo/config/certs2
# 设置参数
CURVE_NAME="prime256v1"
EC_PARAM_FILE="ecdsaparam"
CA_KEY_FILE="maincakey.pem"
CA_CERT_FILE="maincacert.pem"
CA_CONFIG_FILE="maincaconf.cnf"
VALID_DAYS=3650

echo "==> 生成 ECC 参数文件（曲线: $CURVE_NAME）"
openssl ecparam -name "$CURVE_NAME" > "$EC_PARAM_FILE"

echo "==> 基于 ECC 参数生成自签名 CA 根证书（有效期: $VALID_DAYS 天）"
openssl req -nodes -x509 \
  -days "$VALID_DAYS" \
  -newkey ec:"$EC_PARAM_FILE" \
  -keyout "$CA_KEY_FILE" \
  -out "$CA_CERT_FILE" \
  -config "$CA_CONFIG_FILE"

echo "✅ CA 证书生成完成：$CA_CERT_FILE"
echo "私钥文件为：$CA_KEY_FILE"