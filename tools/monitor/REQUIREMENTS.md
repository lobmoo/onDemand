# OnDemand DDS Monitor v2.0 — 需求规格书

> 版本: v2.0-draft
> 日期: 2026-08-14
> 状态: 需求确认中

---

## 1. 项目背景

OnDemand 是一个高性能 pub-sub 系统，管理 10 万+ 变量的 DDS 分发（20 个 bucket，域内通过 RTPS 协议多播）。当前 monitor (run2.py) 是一个 1547 行的单文件工具，存在以下问题：

- Domain ID 无法自动识别，需手动指定
- 页面信息碎片化，Overview 和 RTPS Packets 分离导致信息不聚焦
- 只有协议级统计，缺乏 Topic 级和变量级的深度观测
- 无法定位丢包、重传、分包等关键问题
- 无法观测 OnDemand 应用层的变量发布/订阅细节

本次重构目标：基于 Textual 框架，构建一个**三层递进式**监控工具，从网络发现 → Topic 观测 → 变量级诊断，覆盖全链路。

---

## 2. 用户角色与使用场景

| 角色 | 场景 | 关注点 |
|------|------|--------|
| 开发者 | 联调阶段确认通信链路 | 哪些 Domain 在通信？Topic 对不对？ |
| 运维 | 线上排障 | 哪个 Topic 丢包了？哪个变量延迟高？ |
| 测试 | 压力测试 | 变量发布频率是否达标？订阅是否有遗漏？ |

---

## 3. 功能需求

### 3.1 Page 0 — 网络发现与全局总览

**目标**: 一眼看到环网上所有 RTPS 通信链路的全貌。

#### 3.1.1 Domain 自动发现
- 后台持续抓包，解析所有 SPDP (PDP) 报文
- 从 SPDP DATA 子消息的 inline QoS 中提取 
- 自动汇总当前网络上所有活跃的 Domain ID
- 每个 Domain 显示：
  - Domain ID
  - 活跃 Participant 数量（通过 GUID_PREFIX 去重）
  - 首次发现时间 / 最后活跃时间
  - 包速率 (pkt/s)

#### 3.1.2 端口与通信链路
- 按 Domain 分组，列出每个 Domain 下的通信端口
- 区分 discovery port (7400) 和 user data port (7401)
- 显示每个端口的：
  - 包计数 / 字节计数
  - 实时速率
  - 活跃 Writer ID 列表

#### 3.1.3 原始抓包流（保留现有功能）
- 实时滚动显示抓到的 RTPS 包（最近 N 条）
- 每条显示：时间戳、源IP:端口、目的IP:端口、Domain ID、消息类型、Topic、大小
- 按消息类型着色：SPDP=青色、SEDP=黄色、DATA=绿色、GAP=红色
- 支持暂停/滚动/清除

#### 3.1.4 全局统计面板
- 总包数 / RTPS 包数 / 总字节
- 实时包速率 / 字节速率 / DATA 速率
- 子消息类型分布 (HEARTBEAT / ACKNACK / GAP / DATA / DATA_FRAG)
- 协议健康指标：HB 丢包率、重传率

#### 3.1.5 合并说明
- 原 Page 0 (RTPS Packets) 和 Page 1 (Overview) 合并为新 Page 0
- 上半部分: Domain 发现 + 端口链路（新增）
- 下半部分: 原始抓包流 + 全局统计（合并原有功能）

#### 3.1.6 交互
- 用户可从 Domain 列表中选择一个 Domain ID 进入 Page 1
- 支持键盘上下选择 Domain，回车确认
- 也可直接输入 Domain ID + 端口跳转

---

### 3.2 Page 1 — Topic 观测与协议诊断

**目标**: 针对用户选定的 Domain，深入观测 Topic 级别的通信质量。

#### 3.2.1 进入条件
- 用户在 Page 0 选定 Domain ID（和可选的端口过滤）
- Monitor 只解析该 Domain + 端口范围内的 RTPS 包

#### 3.2.2 Topic 自动发现
- 通过 SEDP DATA 子消息的 inline QoS 自动发现 Topic
  -  (0x0005) 提取 topic name
  -  (0x0007) 提取 type name
- 列出所有发现的 Topic，每个 Topic 显示：
  - Topic 名称
  - 类型名称
  - 关联 Writer ID
  - 包计数 / 字节计数
  - 实时频率 (Hz)
  - 平均包大小

#### 3.2.3 Topic 详情（选中后进入）
用户选择一个 Topic 后，进入详情视图：

**实时收发频率**
- 当前发送频率 (Hz)，基于滑动窗口计算
- 频率变化趋势（最近 N 秒的 ASCII 迷你图或数值序列）

**包大小**
- 当前平均包大小 / 最大包大小 / 最小包大小
- 包大小分布（简单直方图或数值表）

**重传检测 (GAP)**
- 监控 RTPS GAP 子消息
- 统计重传次数和重传率
- GAP 起始 SN 和 gap list

**分包检测 (DATA_FRAG)**
- 监控 DATA_FRAG 子消息
- 统计分包次数
- 显示分片大小和分片数

**丢包检测 (基于 Sequence Number)**
- 跟踪每个 Writer 的 DATA 子消息序列号 (SN)
- 检测 SN 不连续 → 判定丢包
- 统计：
  - 期望 SN vs 实际 SN
  - 丢包次数 / 丢包率
  - 最近丢包的 SN 范围
- 实时告警：丢包率超过阈值时高亮显示

#### 3.2.4 交互
- Topic 列表支持上下选择，回车进入详情
- ESC 返回 Topic 列表
- 再次 ESC 返回 Page 0
- 详情页支持暂停/恢复

---

### 3.3 Page 2 — OnDemand 变量级监控

**目标**: 深入 OnDemand 应用层，观测变量的发布、订阅和传输细节。

#### 3.3.1 变量定义解析 (PubTableDefine)
- 订阅或抓取  Topic
- 解码 IDL 消息 ，提取：
  - 发布节点名 (nodeName)
  - 表名 (name)
  - 变量列表：varHash (u64)、变量名、大小、模型名、所属节点
- 按 Bucket (varHash % 20) 分组统计变量分布

#### 3.3.2 订阅请求解析 (SubTableRegister)
- 订阅或抓取  Topic
- 解码 IDL 消息 ，提取：
  - 订阅节点名 (nodeName)
  - 表名 (tableName)
  - 消息类型 (SUB/UNSUB)
  - 变量订阅列表：变量名 + 请求频率

#### 3.3.3 变量总览
显示全局统计：
- 总发布变量数 / 总订阅变量数
- 活跃 Publisher 节点列表
- 活跃 Subscriber 节点列表
- 每个 Bucket 的变量数（柱状图或数值表）
- 订阅频率分布（多少变量被订阅在 10ms / 100ms / 1s 等频率）

#### 3.3.4 变量列表与搜索
- 列出所有已知变量，显示：varHash、变量名、所属节点、Bucket、大小
- 支持按名称搜索/过滤
- 支持按 Bucket / 节点 / 频率排序

#### 3.3.5 变量订阅观测（高级功能）
用户可以选择某个变量，进入实时观测：
- 该变量的发布频率（实际发送频率 vs 请求频率）
- 该变量所属 Bucket 的 DATA 消息速率
- 变量数据大小变化
- 最近 N 次传输的时间戳序列

#### 3.3.6 数据来源
- **方案 A（抓包模式）**: 通过 RTPS 抓包 + IDL 解码，不依赖 fastdds
- **方案 B（DDS 订阅模式）**: 使用 fastdds Python 绑定直接订阅 Topic
- 优先方案 A，方案 B 作为增强（需安装 fastdds）

---

## 4. 非功能需求

### 4.1 技术栈
- **TUI 框架**: Textual (Python modern TUI framework)
- **抓包**: 原生 socket (AF_PACKET)，不依赖 scapy
- **协议解析**: 自研 RTPS parser（延续 run2.py 的实现）
- **IDL 解码**: 自研 CDR decoder（延续 run2.py 的实现）
- **Python 版本**: 3.8+

### 4.2 性能
- 抓包线程不能因 TUI 渲染阻塞
- TUI 刷新率可配置 (默认 4 Hz)
- 原始包缓冲区限制 (默认 500 条，可配置)
- 滑动窗口速率计算，避免瞬时抖动

### 4.3 可用性
- 无需安装第三方依赖（纯 Python 标准库 + Textual）
- 需要 root 权限（raw socket）
- 启动时自动检测权限和网卡
- 支持  非交互模式（用于脚本/日志）

### 4.4 可维护性
- 模块化文件结构（见目录设计）
- 每个模块职责单一，可独立测试
- 配置集中管理

---

## 5. 页面导航设计

\`\`\`
Page 0: 网络发现 (Domain Discovery)
  │
  ├── 选中 Domain ID → 回车
  │
  ▼
Page 1: Topic 观测 (Topic Monitor)
  │
  ├── 选中 Topic → 回车
  │   ├── Topic 详情 (频率/大小/丢包/重传/分包)
  │   └── ESC 返回 Topic 列表
  │
  ├── 切换到 Page 2
  │
  ▼
Page 2: 变量监控 (Variable Monitor)
  │
  ├── 变量列表 (搜索/过滤/排序)
  ├── 选中变量 → 回车
  │   ├── 变量详情 (频率/大小/时序)
  │   └── ESC 返回变量列表
  │
  └── ESC / Page 0 快捷键返回
\`\`\`

**快捷键设计**:

| 按键 | 功能 |
|------|------|
| 1 / 2 / 3 | 直接跳转 Page |
| ← / → | 左右切换 Page |
| ↑ / ↓ | 列表选择 / 滚动 |
| Enter | 进入选中项详情 |
| ESC | 返回上一级 |
| / | 搜索过滤 (变量页) |
| P | 暂停/恢复 |
| C | 清除统计 |
| Q | 退出 |

---

## 6. 目录结构设计

\`\`\`
tools/monitor_v2/
├── DESIGN.md              # 设计文档
├── REQUIREMENTS.md        # 需求规格书 (本文档)
├── README.md              # 使用说明
├── requirements.txt       # Python 依赖 (textual)
│
├── main.py                # 入口, CLI 参数解析, 启动
├── config.py              # 配置管理 (dataclass)
│
├── core/
│   ├── __init__.py
│   ├── pcap.py            # 抓包线程 (AF_PACKET raw socket)
│   ├── rtps.py            # RTPS 协议解析 (子消息级)
│   ├── cdr.py             # CDR 解码器
│   ├── idl.py             # OnDemand IDL 消息解码 (PubTableDefine 等)
│   ├── metrics.py         # 指标引擎 (RateMeter, 滑动窗口, SN 丢包检测)
│   └── domain_scanner.py  # Domain 自动发现 (SPDP 解析)
│
├── tui/
│   ├── __init__.py
│   ├── app.py             # Textual App 主类
│   ├── page0_discovery.py # Page 0: 网络发现
│   ├── page1_topics.py    # Page 1: Topic 观测
│   ├── page2_variables.py # Page 2: 变量监控
│   ├── widgets/
│   │   ├── __init__.py
│   │   ├── packet_table.py    # 原始抓包表格组件
│   │   ├── domain_list.py     # Domain 列表组件
│   │   ├── topic_detail.py    # Topic 详情组件
│   │   ├── var_list.py        # 变量列表组件
│   │   ├── stats_panel.py     # 统计面板组件
│   │   └── sparkline.py       # 迷你趋势图组件
│   └── styles/
│       └── monitor.tcss       # Textual CSS 样式
│
└── tests/
    ├── test_cdr.py
    ├── test_rtps.py
    ├── test_idl.py
    └── test_metrics.py
\`\`\`

---

## 7. 数据流设计

\`\`\`
                    ┌──────────────┐
                    │  网卡 (NIC)  │
                    └──────┬───────┘
                           │ raw packets
                    ┌──────▼───────┐
                    │  pcap.py     │  AF_PACKET 抓包
                    │  (线程)      │  端口过滤: 7400, 7401
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │  rtps.py     │  RTPS 子消息解析
                    │  cdr.py      │  CDR 序列化解码
                    │  idl.py      │  IDL 消息结构解码
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
       ┌──────▼──────┐ ┌──▼──────────┐ ┌▼─────────────┐
       │domain_scanner│ │metrics.py  │ │ 解码数据      │
       │ SPDP→DomainID│ │ RateMeter  │ │ Topic/Var/Sub │
       │ GUID去重     │ │ SN丢包检测 │ │ PubTableDefine│
       └──────┬──────┘ │ Jitter计算 │ │ SubTableReg   │
              │        └──────┬──────┘ └──────┬───────┘
              │               │               │
              └───────────────┼───────────────┘
                              │
                       ┌──────▼──────┐
                       │  Textual    │  TUI 渲染
                       │  App        │  3 个 Page
                       │  (主线程)   │  事件驱动
                       └─────────────┘
\`\`\`

---

## 8. 验收标准

### Page 0 验收
- [ ] 启动后自动扫描并列出所有 Domain ID
- [ ] 每个 Domain 显示 Participant 数、端口、包速率
- [ ] 选择 Domain 后可进入 Page 1
- [ ] 原始抓包流正常显示，着色正确

### Page 1 验收
- [ ] 自动发现并列出选定 Domain 下的所有 Topic
- [ ] 选择 Topic 后显示实时频率、包大小
- [ ] 基于 SN 检测丢包，丢包率正确计算
- [ ] GAP 统计重传次数
- [ ] DATA_FRAG 统计分包次数

### Page 2 验收
- [ ] 解析 PubTableDefine，正确显示变量列表
- [ ] 解析 SubTableRegister，正确显示订阅信息
- [ ] 变量按 Bucket 分组统计正确
- [ ] 支持变量搜索/过滤
- [ ] 选择变量后可查看实时观测数据

---

## 9. 里程碑计划

| 阶段 | 内容 | 交付物 |
|------|------|--------|
| M1 | 基础框架 + 抓包 + rtps/cdr 模块 | core/ 全部模块 + 单元测试 |
| M2 | Page 0: Domain 发现 + 原始抓包 | page0_discovery.py |
| M3 | Page 1: Topic 发现 + 协议诊断 | page1_topics.py |
| M4 | Page 2: 变量监控 + OnDemand IDL | page2_variables.py |
| M5 | 集成测试 + 样式优化 + 文档 | 完整可用版本 |
\`\`"
