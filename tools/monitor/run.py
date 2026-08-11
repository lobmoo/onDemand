#!/usr/bin/env python3
"""
独立启动脚本 - 不依赖包路径, 直接 python3 run.py 运行
用法:
  cd tools/monitor && sudo python3 run.py --interface lo --domain 66
"""
import os
import sys

# 把 monitor 目录加到 path, 这样相对导入变绝对导入
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# 把项目根目录加到 path (给 scapy 等用)
if "/usr/local/lib/python3.8/dist-packages" not in sys.path:
    sys.path.insert(0, "/usr/local/lib/python3.8/dist-packages")

from main import main

if __name__ == "__main__":
    main()
