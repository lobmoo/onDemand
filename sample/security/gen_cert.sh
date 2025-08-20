#!/bin/bash
set -e
cd /home/weiqb/src/test_demo/config/certs

# 参数默认值
KEY_OUT=""
CERT_OUT=""
SUBJECT=""
DAYS=3650

EC_PARAM_FILE="ecdsaparam"
CSR_FILE="req.pem"
CA_CERT="maincacert.pem"
CA_KEY="maincakey.pem"
SERIAL_FILE="ca.srl"

# ✅ 检查 CA 文件是否存在
if [[ ! -f "$CA_CERT" || ! -f "$CA_KEY" ]]; then
  echo "❌ 找不到 CA 证书或私钥文件："
  [[ ! -f "$CA_CERT" ]] && echo "   缺失文件：$CA_CERT"
  [[ ! -f "$CA_KEY" ]] && echo "   缺失文件：$CA_KEY"
  exit 1
fi

# 解析参数
while [[ "$#" -gt 0 ]]; do
  case "$1" in
    --keyout)
      KEY_OUT="$2"
      shift 2
      ;;
    --out)
      CERT_OUT="$2"
      shift 2
      ;;
    --subj)
      SUBJECT="$2"
      shift 2
      ;;
    *)
      echo "❌ 未知参数: $1"
      echo "用法:"
      echo "  $0 --keyout <subkey.pem> --out <subcert.pem> --subj <简写或完整DN>"
      echo "示例:"
      echo "  $0 --keyout subkey.pem --out subcert.pem --subj sub"
      exit 1
      ;;
  esac
done

# 参数校验
if [[ -z "$KEY_OUT" || -z "$CERT_OUT" || -z "$SUBJECT" ]]; then
  echo "❌ 缺少必需参数。"
  echo "用法:"
  echo "  $0 --keyout <subkey.pem> --out <subcert.pem> --subj sub"
  exit 1
fi

# 如果 SUBJECT 是别名，则拼接成完整 DN
case "$SUBJECT" in
  /*)
    # 已经是完整 DN，直接用
    ;;
  *)
    # 简写别名，自动构造 DN 字符串
    SUBJECT="/C=CN/ST=SH/L=BaoSight/O=DSFconncetor/CN=${SUBJECT}/emailAddress=DSFconncetor@baosight.com"
    ;;
esac

# 步骤 1: 生成 ECC 参数
openssl ecparam -name prime256v1 > "$EC_PARAM_FILE"

# 步骤 2: 生成私钥和 CSR
echo "==> 生成私钥 $KEY_OUT"
openssl req -nodes -new \
  -newkey ec:"$EC_PARAM_FILE" \
  -keyout "$KEY_OUT" \
  -out "$CSR_FILE" \
  -subj "$SUBJECT"

# 步骤 3: 使用 CA 文件签发证书
echo "==> 使用 CA 签发证书：$CERT_OUT"
openssl x509 -req \
  -in "$CSR_FILE" \
  -CA "$CA_CERT" \
  -CAkey "$CA_KEY" \
  -CAcreateserial \
  -CAserial "$SERIAL_FILE" \
  -out "$CERT_OUT" \
  -days "$DAYS" \
  -sha256

echo "✅ 证书签发成功：$CERT_OUT"

# 清理临时文件
rm -f "$SERIAL_FILE" "$EC_PARAM_FILE" "$CSR_FILE"