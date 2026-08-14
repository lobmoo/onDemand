# OnDemand DDS Monitor v2.0

基于 Textual 框架的三层递进式 DDS 监控工具。

## 安装依赖

```bash
# 方式 1: apt
sudo apt-get install -y python3-pip
pip3 install textual

# 方式 2: get-pip.py (无需 apt)
curl -sS https://bootstrap.pypa.io/pip/3.8/get-pip.py -o /tmp/get-pip.py
sudo python3 /tmp/get-pip.py
pip3 install textual
```

## 运行

```bash
cd ~/workspace/onDemand/tools/monitor_v2
sudo python3 main.py -i enx0826ae3a20b7
sudo python3 main.py -i enx0826ae3a20b7 -d 166
```

## 页面导航

| 按键 | 功能 |
|------|------|
| 1 / 2 / 3 | 跳转到对应页面 |
| left / right | 左右切换页面 |
| Enter | 进入选中项详情 |
| ESC | 返回上一级 |
| P | 暂停/恢复 |
| C | 清除统计 |
| Q | 退出 |

### Page 0 — 网络发现
- 自动扫描环网所有 RTPS 链路
- 从 SPDP 报文解析 Domain ID
- 按 Domain 分组显示端口、Participant、包速率
- 底部显示原始抓包流（着色）

### Page 1 — Topic 观测
- 列出选定 Domain 下的所有 Topic（SEDP 发现）
- 选中 Topic 查看详情：频率、大小、丢包(SN)、重传(GAP)、分包(FRAG)

### Page 2 — 变量监控
- 解析 PubTableDefine / SubTableRegister
- 变量列表：名称、Bucket、大小、节点
- 订阅信息：节点、频率、变量数

## 代码结构

```
monitor_v2/
├── main.py                # 入口
├── config.py              # 配置
├── core/
│   ├── cdr.py             # CDR 解码器
│   ├── rtps.py            # RTPS 协议解析
│   ├── idl.py             # OnDemand IDL 消息解码
│   ├── metrics.py         # 指标引擎 (RateMeter, SNTracker)
│   └── pcap.py            # 抓包线程
├── tui/
│   ├── app.py             # Textual App + 3 个 Page
│   └── styles/
│       └── monitor.tcss   # Textual CSS
└── requirements.txt
```

## 与旧版的关系

- `monitor/` — 旧版 (run2.py 单文件，curses TUI)
- `monitor_v2/` — 新版 (模块化，Textual TUI)
- 旧版代码保留不动