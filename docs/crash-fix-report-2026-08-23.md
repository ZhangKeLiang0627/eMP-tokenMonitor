# eMP_tokenMonitor 闪退问题排查与修复报告（2026-08-23）

## 1. 问题现象

应用在 T113-S3 板子上运行一段时间（10-50 小时）后"无缘无故闪退"，进程消失、无任何异常日志。
用户抓取的日志最后一条为：

```
[2026-08-22 04:14:19.020] [tid 1446] [debug] [DeepSeekClient.cpp:74:checkNetwork]
[Net] checkNetwork: connect failed, error=8
```

此后日志戛然而止，进程消失。

## 2. 根因定位过程

### 2.1 板端 dmesg 取证：OOM killer 实锤

板端 `dmesg` 中检索到 **3 条 OOM killer 记录**，均为本进程被杀：

```
eMP_tokenMonito invoked oom-killer: gfp_mask=0x100cca(GFP_HIGHUSER_MOVABLE), order=0, oom_score_adj=0
Out of memory: Killed process 1784 (eMP_tokenMonito) total-vm:93260kB, anon-rss:80396kB, ...
Out of memory: Killed process 989 (eMP_tokenMonito) total-vm:92264kB, anon-rss:79508kB, ...
Out of memory: Killed process 1443 (eMP_tokenMonito) ...
```

- 板子内存：`MemTotal = 115720 kB`（128MB DDR，可用约 105MB）
- 进程正常运行时 RSS ≈ 10MB，被 OOM 杀死时 **anon-rss 高达 79-80MB**
- 结论：**内存泄漏长期累积触发内核 OOM killer，进程被静默杀死**（SIGKILL 无法捕获，故无日志）

### 2.2 泄漏速率实测（对照实验链）

在板端（TinaLinux + **OpenSSL 1.1.0i** + musl）逐一做对照实验：

| 实验 | 方法 | 结果 |
|---|---|---|
| 应用压测 | httplib 每 3s 全 HTTPS 请求 | RSS 每 30s 涨 600-900KB（约 25KB/s） |
| 裸 OpenSSL | SSL_new/SSL_connect/SSL_free 循环 600 次 | RSS 线性增长，每次连接泄漏 ≈ 4-5KB |
| 单连接复用 | 1 个 TLS 连接 keep-alive 发 80 个请求 | **无额外泄漏**（连接内请求不泄漏） |
| httplib 复用 Client | 复用同一 Client 连续请求 | **仍泄漏**，且 /proc/net/tcp 显示本地端口每次变化（连接未复用） |
| 修复后复用 | `set_keep_alive(true)` + 探测补丁 | 同一本地端口稳定保持，RSS 平线 |

### 2.3 真正的元凶：httplib 客户端默认不发 keep-alive

阅读 httplib 0.26.0 源码确认：

- `ClientImpl::keep_alive_` 默认 **false**（`httplib.h:1643`）
- `write_request()` 中客户端默认设置请求头 `Connection: close`（`httplib.h:8787` 附近 `auto close_connection = !keep_alive_;`）

因此**每一次 HTTP(S) 请求都会新建 TCP+TLS 连接**。在板端旧版 OpenSSL 1.1.0i 下，
每次 TLS 建连泄漏 ≈ 4-5KB（OpenSSL 库级泄漏，已在 1.1.1+ 修复），叠加应用每 3s 探测 +
每 30s 拉取的频率，泄漏速率 ≈ 1.3-5KB/s ≈ 4.6-18MB/h，运行 10-50 小时后内存耗尽被 OOM 杀死。

> 次要因素：httplib 的 `is_ssl_peer_could_be_closed()` 用 `SSL_peek` 探测空闲连接，
> OpenSSL 1.1.0i 在空闲 keep-alive 连接上会误报 `SSL_ERROR_ZERO_RETURN`（对端已关闭），
> 即使开启 keep-alive 也会导致连接被误弃重建，需一并修补。

### 2.4 顺带发现：退出路径并发竞态（ASan 实证）

对应用做 AddressSanitizer 构建后运行，SIGTERM 退出时抓到两个错误：

- **double-free**：SIGTERM 等信号可递交给进程内任意线程，多个线程并发进入
  `signalExitCallback` 并同时调用 `exit(0)`，glibc exit handlers 被双重执行
- **heap-use-after-free**：退出时 `Model` 已析构，数据线程仍在访问
  `DeepSeekClient::createClient()` 成员

## 3. 修复方案

| # | 修复点 | 文件 | 说明 |
|---|---|---|---|
| 1 | 显式开启 keep-alive | `src/Net/DeepSeekClient.cpp` | `set_keep_alive(true)`，让 TLS 连接真正复用 |
| 2 | Client 单实例复用 | `inc/DeepSeekClient.h`、`src/Net/DeepSeekClient.cpp` | `getClient()` 惰性创建，SSL_CTX 只建一次 |
| 3 | TimeSync 静态复用 | `src/Net/TimeSync.cpp` | 两个对时源均改用 `static` 复用客户端 |
| 4 | httplib 空闲探测补丁 | `libs/cpp-httplib/httplib.h` | `is_ssl_peer_could_be_closed()` 仅当 socket 有数据可读时才做 SSL 层探测，空闲连接视为存活（函数内 `WorkBuddy patch` 注释） |
| 5 | 探测降频 | `src/Page/Model.cpp` | `NETWORK_PROBE_INTERVAL_SEC` 3 → 10 |
| 6 | 信号退出单次执行 | `src/HAL/HAL.cpp` | `std::atomic_flag` 保证 `signalExitCallback` 只执行一次 |
| 7 | 退出不再 delete Model | `main.cpp` | 退出按钮回调统一 `exit(0)`，由内核回收资源，消除与数据线程/LVGL 线程的并发析构竞态 |

## 4. 验证结果

### 4.1 SDL 仿真（Ubuntu VM，xvfb 无头）

- 编译：`make CROSS=0` 通过
- 运行：`EMP_AUTOSHOT=5` 自动截图成功（35600 字节 PNG）
- 退出：SIGTERM 干净退出（`Got signal 15, exiting`），**无 double-free / use-after-free**
- ASan 构建（`-fsanitize=address`）运行无报告

### 4.2 T113-S3 板端（192.168.124.11）

- 交叉编译：`make CROSS=1` 产出 ARM 32-bit ELF（约 1.8MB）
- **RSS 稳定性（修复前后对比）**：

```
修复前（每 3s 建连）:  每 30s RSS +600-900KB，12 分钟增长约 7.7MB
修复后（keep-alive）:  12 分钟 RSS 精确平线 9640KB（逐分钟采样完全不变）
```

- **连接复用确认**：`/proc/net/tcp` 中到 443 端口的 ESTABLISHED 连接本地端口
  在连续 60 次请求期间稳定保持（修复前每次请求端口变化）
- 服务器响应头确认 `Connection: keep-alive`
- 进程 12 分钟持续 ALIVE，截图正常，`fetchBalance: OK` 与 `TimeSync` 周期正常

### 4.3 预期效果

- 连接建立频率：从"每请求 1 次"降为"每连接生命周期 1 次"，泄漏基本消除

## 5. 遗留说明

- 板端 OpenSSL 1.1.0i（2018 年）为 tina-sdk 自带，其库级泄漏无法通过应用层彻底根除；
  本方案通过连接复用根治泄漏源头
- 若后续升级板端 OpenSSL 至 1.1.1+，可进一步减少连接维护开销
