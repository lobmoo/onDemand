# OnDemand DDS Monitor

## 概述
本工具用于监控 OnDemand DDS 系统的网络层、协议层和应用层指标。

## 架构
- **Layer 1 (网络层)**: scapy + rtps_parser.py (UDP 抓包与 RTPS 协议解析)
- **Layer 2 (DDS 应用层)**: fastdds (订阅 tableDefine, bucket_N 等 Topic)
- **Layer 3 (展示层)**: curses + rich (交互终端 TUI)

## 运行方法
\`\`\`bash
# 在远程服务器 192.168.137.100 上运行
# 1. 确保 demo 已在 Docker 中启动 (ID: 0925d0950a07)
# 2. 进入监控目录
cd ~/workspace/onDemand/tools/monitor
# 3. 运行监控 (需 sudo 权限用于抓包)
sudo python3 run2.py -i enx0826ae3a20b7 -d 166
\`\`\`

## 当前测试环境
- **Demo 运行环境**: Docker 容器 \`0925d0950a07\`
- **DDS Domain ID**: \`166\`
- **监控网卡接口**: \`enx0826ae3a20b7\`
- **运行指令**: \`sudo python3 run2.py -i enx0826ae3a20b7 -d 166\`

## 代码结构
- \`run2.py\`: 实际使用的单文件监控工具 (1547行，原生socket抓包 + curses TUI)
- \`run.py\` + \`main.py\` + \`layer*.py\`: 设计文档中的模块化架构 (未完成)

## 视图切换
- **[0]** RTPS Packets (协议包详情)
- **[1]** Overview (总览)
- **[2]** Writers (写入端统计)
- **[3]** Protocol (协议健康)
- **[4]** Topics (DDS Topic 发现)
- **[5]** Variables (变量定义与订阅)

## 交互控制
- **←/→**: 切换视图
- **↑/↓**: 滚动
- **P**: 暂停/恢复
- **C**: 清除统计
- **Q**: 退出

## 依赖
- Python 3.8+
- 需要 root 权限 (原生 socket 抓包)
- 无第三方依赖 (原生 socket 实现，不用 scapy/fastdds)
