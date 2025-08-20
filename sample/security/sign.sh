#!/bin/bash
set -e
cd /home/weiqb/src/test_demo/config/certs
# 签名用的证书和私钥
CERT="maincacert.pem"
KEY="maincakey.pem"

# 要签名的 XML 文件
FILES=("governance.xml" "permissions.xml")

for file in "${FILES[@]}"; do
  if [[ ! -f "$file" ]]; then
    echo "⚠️ 跳过：未找到 $file"
    continue
  fi

  out_file="${file%.xml}.smime"
  echo "🔏 正在签名 $file → $out_file"

  openssl smime -sign \
    -in "$file" \
    -text \
    -out "$out_file" \
    -signer "$CERT" \
    -inkey "$KEY"

  echo "✅ 签名完成: $out_file"
done