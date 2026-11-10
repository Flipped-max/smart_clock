# ESP32-S3 AI 智能桌面语音时钟 (AI Smart Clock)

基于 **ESP32-S3** (N8R8) 的多功能嵌入式桌面智能终端，集成了 **ESP-SR 离线语音唤醒**、**VAD 智能人声断句**、**LVGL 8.3 现代化图形界面**、**SNTP 毫秒级网络授时** 以及 **Open-Meteo 实时气象数据流式展示**。

---

## 🌟 功能特性

- **🎙️ 离线语音唤醒与识别**：集成乐鑫官方 `esp-sr` (WakeNet 9) 模型，无需联网即可实现毫秒级快速离线语音唤醒（唤醒词：“Hi 乐鑫”）。
- **🗣️ AFE 降噪与自适应 VAD 断句**：利用音频前端（AFE）进行噪声抑制与语音活动检测（VAD），检测到用户停止说话（约 0.8 秒连续静音）时自动结束录音，告别死板的固定时长录制。
- **🧠 外部 8MB PSRAM 高效录制**：在外部 Octal PSRAM 中开辟 160KB 音频主缓冲（5 秒 16kHz 16-bit 线性 PCM），极大释放芯片片上 Internal SRAM 压力。
- **🖥️ 现代化图形界面 (LVGL 8.3)**：基于 `esp_lvgl_port` 驱动 ST7789 屏幕，设计深色科技感表盘；通过 LittleFS 动态挂载与加载外部中文字库（`font_cn_16.bin`），降低固件代码体积。
- **⚡ 双核异构并发调度**：
  - **Core 1**：独占运行 I2S DMA 音频喂流与 ESP-SR 神经网络推理，保障高实时性与零丢帧。
  - **Core 0**：承载 Wi-Fi 协议栈、SNTP 授时、HTTPS 客户端请求以及 UI 消息队列渲染任务。
- **🌐 网络授时与气象云服务**：
  - 自动连接 Wi-Fi（支持 WPA2/WPA3 与断网自愈）。
  - 对接阿里云 NTP 授时服务，毫秒级校准北京时间（CST-8）。
  - HTTPS 安全请求 Open-Meteo 实时气象接口，cJSON 流式解析温度、天气状况、风速，并通过 FreeRTOS 队列以“打字机”流式效果展示在屏幕上。

---

## 🛠️ 硬件清单与引脚定义

| 外设模块 | 信号引脚 | ESP32-S3 GPIO | 说明 |
| :--- | :--- | :--- | :--- |
| **ST7789 LCD** (240×296) | SCLK | GPIO 12 | 40MHz 高速 SPI 时钟 |
| | MOSI | GPIO 11 | SPI 数据发送 |
| | CS | GPIO 10 | 片选引脚 |
| | DC | GPIO 9 | 数据/命令控制 |
| **INMP441 麦克风** | SCK (BCLK) | GPIO 41 | I2S 位时钟 |
| | WS (LRCK) | GPIO 42 | I2S 左右声道时钟 |
| | SD (DIN) | GPIO 2 | I2S 串行音频数据 |

---

## 📂 项目目录结构

```text
smart_clock/
├── components/
│   ├── app_ui/             # LVGL 界面设计、LittleFS 字库挂载、UI 消息队列消费者
│   ├── bsp_board/          # 硬件板级驱动 (ST7789 SPI 液晶驱动、INMP441 I2S 驱动)
│   ├── mid_audio_sr/       # ESP-SR 语音中间件 (Feed 任务、Detect 任务、VAD 断句)
│   └── mid_net_ai/         # Wi-Fi STA、SNTP 校时、Open-Meteo 接口与流式文本推送
├── fs_data/                # LittleFS 文件系统镜像资源 (含 font_cn_16.bin 中文字库)
├── main/                   # 主程序入口 (main.c、app_common.h 全局状态定义)
├── partitions.csv          # 自定义分区表 (含 model 分区与 3MB littlefs 分区)
├── CMakeLists.txt          # 工程顶层 CMake 配置文件
└── sdkconfig               # ESP-IDF 详细配置 (PSRAM、CPU 主频、LVGL 配置等)
```

---

## 🚀 快速上手

### 1. 环境准备
- 推荐开发框架：**ESP-IDF v5.1+**
- 编译工具链：CMake / Ninja
- 硬件：ESP32-S3 开发板（带 8MB Flash 与 8MB Octal PSRAM，如 N8R8）

### 2. 配置 Wi-Fi 与城市气象
在编译前，打开 `components/mid_net_ai/include/mid_net_ai.h`，修改你的 Wi-Fi 热点信息及目标城市经纬度：

```c
/*===============WiFi设置================*/
#define WIFI_STA_SSID       "YOUR_WIFI_SSID"
#define WIFI_STA_PASSWORD   "YOUR_WIFI_PASSWORD"

/*==============Open_Meteo免费天气========*/
#define OPEN_METEO_CITY     "太原"
#define OPEN_METEO_URL      "https://api.open-meteo.com/v1/forecast?latitude=37.87&longitude=112.56&current=temperature_2m,weather_code,wind_speed_10m"
```

### 3. 编译与烧录
在终端中进入项目根目录：

```bash
# 1. 设置目标芯片
idf.py set-target esp32s3

# 2. 编译工程（首次编译会自动下载 esp-sr 与 lvgl 依赖组件）
idf.py build

# 3. 烧录固件与监控日志（请将 COMx 替换为你的串口号）
idf.py -p COMx flash monitor
```

---

## 📜 许可证

本项目基于 MIT 许可证开源，欢迎自由学习与改进。
