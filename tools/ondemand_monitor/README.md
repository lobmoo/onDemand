# OnDemand DDS Monitor

实时 RTPS 流量分析和监控工具，用于 OnDemand 发布订阅系统。

## 功能特性

- **实时抓包**：基于 libpcap 的 UDP 流量捕获
- **RTPS 协议解析**：解析 DATA、HEARTBEAT、ACKNACK 子消息
- **指标统计**：
  - 参与者发现（Participant Discovery）
  - 端点信息（DataWriter/DataReader）
  - 传输统计（数据包、字节、NACK 率）
- **可视化 TUI**：基于 FTXUI 的终端界面
  - Overview 页面：总览统计
  - Participants 页面：参与者列表
  - Endpoints 页面：端点详情
  - Transfer 页面：传输统计

## 环境要求

- Ubuntu 20.04+
- g++ 9.4+ (C++17)
- CMake 3.14+
- libpcap-dev
- FastDDS (fastcdr)
- FTXUI (自动下载)

## 安装依赖

```bash
# Ubuntu/Debian
sudo apt install libpcap-dev libncurses-dev

# 如果使用 FastDDS (已包含 fastcdr)
# 无需额外安装
```

## 编译

```bash
cd tools/ondemand_monitor
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 使用

```bash
# 需要 root 权限（抓包）
sudo ./ondemand_monitor -i lo

# 指定网络接口
sudo ./ondemand_monitor -i eth0

# 自定义过滤器
sudo ./ondemand_monitor -i lo -f 'udp port 7410'
```

### 命令行参数

```
-i, --interface <name>    网络接口 (默认: any)
-f, --filter <expr>       BPF 过滤器 (默认: udp port 7410 or udp port 7411)
-h, --help                显示帮助
```

### 快捷键

- `Tab` / `1-4`：切换页面
- `↑/↓`：导航列表
- `q` / `ESC`：退出

## 页面说明

### 1. Overview（总览）

显示系统整体统计：
- 参与者数量
- 端点数量
- 数据消息总数
- 总字节数
- Heartbeat/ACKNACK/NACK 计数

### 2. Participants（参与者）

显示发现的 DomainParticipant 列表：
- GUID
- 端点数量
- 首次/最后发现时间

### 3. Endpoints（端点）

显示选中参与者的所有 DataWriter/DataReader：
- GUID
- Topic 名称
- 类型名称
- 方向（Writer/Reader）
- 统计数据

### 4. Transfer（传输）

显示端点对之间的传输统计：
- Writer/Reader GUID
- 数据包数量
- 字节数
- NACK 率
- 最后传输时间

## 架构

```
┌─────────────────────────────────────────────────────────┐
│ MonitorUi (FTXUI TUI)                                    │
└────────────┬────────────────────────────────────────────┘
             │ 100ms 刷新
┌────────────▼────────────────────────────────────────────┐
│ MetricsEngine: 核心数据模型                               │
│  - Participants / Endpoints / TransferStats              │
└────────────┬────────────────────────────────────────────┘
             │ RtpsMessage
┌────────────▼────────────────────────────────────────────┐
│ RtpsParser: RTPS 报文解析器                               │
│  - ParseHeader / ParseSubmessages                        │
└────────────┬────────────────────────────────────────────┘
             │ UdpPacket
┌────────────▼────────────────────────────────────────────┐
│ PcapWorker: libpcap 抓包线程                              │
│  - 单独线程 + 无锁队列                                    │
└─────────────────────────────────────────────────────────┘
```

## 线程模型

1. **PcapWorker 线程**：抓包并推入队列
2. **主线程**：
   - 处理键盘输入
   - 从队列 pop 包并解析
   - 更新 MetricsEngine
   - 刷新 UI

## 测试

### 启动测试

1. 启动 OnDemand 发布端：
```bash
cd build && ./demo_exec
```

2. 启动监控：
```bash
sudo ./ondemand_monitor -i lo
```

3. 观察 TUI 显示：
   - Overview 页面应显示参与者和端点
   - Participants 页面应列出发现的参与者
   - Endpoints 页面应显示 topic 信息

### 单元测试

```bash
cd build
make test
```

## 后续扩展

1. **JSON 导出**：支持文件/HTTP/WebSocket 输出
2. **离线分析**：支持读取 pcap 文件
3. **告警系统**：丢包率超阈值告警
4. **QoS 兼容性检查**：完整实现 Reliability/Durability/Ownership 检查
5. **Topic 数据解析**：解析 TableDataTransfer payload

## License

Internal tool - see project root for license.
