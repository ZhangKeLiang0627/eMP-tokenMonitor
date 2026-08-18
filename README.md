# eMP_tokenMonitor

DeepSeek Token / 余额监视器（LVGL 界面），沿用 [eMP-mainPage](https://github.com/ZhangKeLiang0627/eMP-mainPage) 与 [eMP-settings](https://github.com/ZhangKeLiang0627/eMP-settings) 的目录结构、编译规范与 UI 设计风格。

## 功能

- 定时请求 DeepSeek 余额接口 `GET https://api.deepseek.com/user/balance`
- 网络连通性检测（网络状态点实时显示）
- 优雅展示：总余额、币种、赠金余额、充值余额、是否可用
- 支持手动刷新、截图（Shot 按钮）
- 基于 `cpp-httplib`（HTTPS）发起请求，`nlohmann/json` 解析响应

## 环境

```shell
# 交叉编译需要（请按本机实际路径修改）
export STAGING_DIR=/home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target
```

本地编译依赖：

```bash
sudo apt install build-essential libsdl2-dev libfreetype6-dev libncurses5-dev libssl-dev
```

## 配置

编辑 `./config/sysconfig.json`：

```json
{
  "api_key": "sk-你的DeepSeek密钥",
  "refresh_interval_sec": 30
}
```

- `api_key`：DeepSeek API Key（**必填**）
- `refresh_interval_sec`：余额拉取周期，单位秒（最小 5 秒）

## 编译

- Makefile
```shell
# 交叉编译（目标板 T113-S3）
./build.sh

# 本地编译（PC 模拟器）
./build.sh 0
```

- CMake
```shell
# 本地编译
mkdir build && cd build
cmake ..
make -j32
```

## 运行

可执行文件为：`eMP_tokenMonitor`

```shell
./eMP_tokenMonitor
```

> 本地编译需要 `/mnt/UDISK/font/SmileySans.ttf` 中文字体，缺失时自动回退为英文界面（montserrat 字体）。

## 文件

- `./config/sysconfig.json`：API Key 与拉取周期配置
- `./src/Net/DeepSeekClient.cpp`：DeepSeek 余额请求（cpp-httplib + nlohmann/json）
- `./src/Page/View.cpp`：界面（延续 eMP 系列风格）
- `./src/Page/Model.cpp`：数据线程 / 定时刷新 / 状态管理

## 截图

![screenshot](./pictures/image-1.png)
