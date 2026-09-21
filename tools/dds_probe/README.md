# dds_probe — RTPS 流量监控工具

基于 libpcap 的实时抓包分析工具，分析 OnDemand 发布/订阅系统的 RTPS 流量，自带 TUI 界面和可选 HTTP JSON 接口。

---

## 1. 前置条件

| 依赖 | 说明 |
|------|------|
| g++ 9.4+（C++17） | 编译需要 |
| CMake 3.14+ | 构建 |
| libpcap | 抓包库，**必须装** |

```bash
sudo apt-get install libpcap-dev
```

> 自查：`ldconfig -p | grep libpcap`，能查到 `libpcap.so.0.8` 即运行不缺库。

---

## 2. 编译

在**项目根目录**顶层构建（`tools/dds_probe` 是顶层 CMake 子目录，BUILD_MONITOR 默认 ON）：

```bash
cd /home/wwk/workspace/onDemand
cd build
cmake .. -DUSE_FASTDDS=ON        # FASTDDS / TXDDS 二选一
make -j$(nproc)
```

产物：`build/tools/dds_probe/dds_probe`

---

## 3. 运行

### 在线抓包（需 root）

```bash
# demo 流量走 lo 回环
sudo ./build/tools/dds_probe/dds_probe -i lo

# 抓指定网卡
sudo ./build/tools/dds_probe/dds_probe -i eth0

# 自定义 BPF 过滤
sudo ./build/tools/dds_probe/dds_probe -i lo -f 'udp port 7410'
```

### 离线回放 pcap（无需 root）

```bash
./build/tools/dds_probe/dds_probe -r capture.pcap
./build/tools/dds_probe/dds_probe -r capture.pcap -f 'udp port 7410'
```

### 同时开 HTTP JSON 接口

```bash
sudo ./build/tools/dds_probe/dds_probe -i lo -p 8080
```

启动成功打印 `HTTP JSON endpoint on :8080`，接口对接见 [HTTP_API.md](HTTP_API.md)。

---

## 4. 命令行参数

```
-i, --interface <name>    网络接口（默认 any）
-r, --read <file>         离线读取 pcap 文件（非空 = 离线模式）
-f, --filter <expr>       BPF 过滤表达式（默认 udp）
-p, --http-port <port>    HTTP JSON 接口端口（默认关闭）
-d, --dump                headless 离线诊断（此时不开 HTTP）
-h, --help                显示帮助
```

### 快捷键

- `↑/↓` / `j/k`：导航参与者列表
- `Enter`：查看参与者详情
- `ESC`：返回列表
- `q`：退出

---

## 5. 常见问题

| 现象 | 解决 |
|------|------|
| `sudo: 找不到命令` | 用完整路径：`sudo ./build/tools/dds_probe/dds_probe` |
| 编译报 `libpcap not found` | `sudo apt-get install libpcap-dev` |
| 界面空白没数据 | 先启动核心 demo 产生流量；接口选错时用 `-i lo` |
| `-p` 端口被占用 | 换端口，或 `sudo lsof -i :8080` 查占用 |

---

## 6. 输出

TUI 页面：Overview（总览）/ Participants（参与者）/ Endpoints（端点）/ Transfer（传输）。
首页顶部显示 **Net BW**（网卡总带宽）：↓ 入向 / ↑ 出向 / = 合计，采样 `/sys/class/net` 计数器，默认 `-i any` 时汇总所有非 lo 网卡；离线回放（`-r`）时显示 `--`。
HTTP 接口：见 [HTTP_API.md](HTTP_API.md)。