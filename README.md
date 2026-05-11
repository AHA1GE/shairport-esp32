# shairport-esp32 (fork)

## 简介

这是对 nel-luke/shairport-esp32 的分叉并修复后的版本，运行于 ESP32 系列芯片上，作为一个 AirPlay（RAOP）接收端。项目支持通过 RTSP/RTP 接收音频流、ALAC 解码，并通过 I2S 输出音频，同时通过 mDNS 广播服务以供发现。

## 概要

- 修复了 Mbed TLS 4 / PSA 迁移后导致的 RSA 私钥映射问题（参考 [main/common.c](main/common.c#L104））。
- 将 ALAC 解码从 RTP 路径上迁移到独立任务，以避免栈空间不足导致的崩溃（参考 [main/player.c](main/player.c#L216)）。
- 改进了多设备切换时的会话回收逻辑，确保旧的 RTSP/RTP/Player 会话被可靠关闭，避免 owner、socket 与解码状态串线（参考 [main/rtp.c](main/rtp.c#L57) 和 [main/rtsp.c](main/rtsp.c#L93)）。
- 优化了同步边界：将会与 RTSP 线程互相干扰的 task notification 改为独立的 semaphore；并修复了音量处理在 semaphore 未建立时的空指针问题（参考 [main/player.c](main/player.c#L554)）。

## 特性

- 通过 RTSP/RTP 接收 AirPlay 音频流
- ALAC 解码
- I2S 输出
- mDNS 广播服务
- 多设备切换

## 编译

1. 按 Espressif 官方文档安装并配置 ESP-IDF 开发环境（包括交叉编译工具链、Python 依赖等）。
2. 在本项目根目录下执行构建命令。

更多详细的环境配置和平台说明，请参见 ESP-IDF 官方文档。

## 备注

- 本仓库为分叉修复版，基于原项目但对新版 ESP-IDF / Mbed TLS 做了兼容性与稳定性改进。
- 如需定制 I2S 引脚、音量或输出设备，请使用 `idf.py menuconfig` 或直接修改 `main/` 下对应驱动代码。
