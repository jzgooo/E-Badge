# ESP32-S3-Touch-AMOLED-1.75C 硬件规格

本仓库对应硬件为微雪（Waveshare）**ESP32-S3-Touch-AMOLED-1.75C** 电子吧唧造型圆形 AMOLED 开发板。

| SKU | 版本 |
| --- | --- |
| 33691 | ESP32-S3-Touch-AMOLED-1.75C（带锂电池） |
| 33692 | ESP32-S3-Touch-AMOLED-1.75C-EN（不带锂电池） |

## 核心与存储

| 项目 | 规格 |
| --- | --- |
| MCU | ESP32-S3R8，Xtensa 32 位 LX7 双核，最高 240 MHz |
| SRAM / ROM | 512 KB SRAM + 384 KB ROM |
| PSRAM | 叠封 8 MB（Octal SPI，示例配置 80 MHz） |
| Flash | 外接 32 MB NOR Flash |
| 无线 | 2.4 GHz Wi-Fi 802.11 b/g/n（支持 40 MHz 带宽）、Bluetooth 5 (LE) / Mesh，板载天线 |

本仓库 ESP-IDF 示例按 32 MB Flash 配置（`CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y`）。官方文档「特性」条目曾写 16 MB Flash，与产品页、板载资源图及本仓库配置不一致，以 **32 MB** 为准。

## 显示与触摸

| 项目 | 规格 |
| --- | --- |
| 屏幕 | 1.75 英寸圆形电容 AMOLED |
| 分辨率 | 466 × 466 |
| 色深 | 16.7M（1670 万色） |
| 亮度 | 700 cd/m² |
| 对比度 | 100000:1 |
| 视角 | 178° |
| 显示驱动 | CO5300，QSPI |
| 触摸 | CST9217，I2C 电容触控 |

BSP 中分辨率为 `BSP_LCD_H_RES` / `BSP_LCD_V_RES` = 466。

## 板载外设

| 功能 | 器件 / 接口 | 说明 |
| --- | --- | --- |
| 电源管理 | AXP2101 | 充电、多路电压输出、电池管理 |
| 电池 | MX1.25 2PIN | 3.7 V 锂电池充放电接口；可选随包装附带电池 |
| 运动传感器 | QMI8658 | 六轴 IMU（三轴加速度 + 三轴陀螺仪） |
| 音频输出 | ES8311 | 编解码芯片 + 板载扬声器焊盘 |
| 音频输入 | 双麦克风 + ES7210 | 麦克风阵列与回声消除 |
| 按键 | PWR、BOOT | 侧边按键，可自定义功能 |
| USB | Type-C | ESP32-S3 原生 USB，用于烧录与日志 |
| 扩展 | I2C / UART / GPIO 焊盘 | 无 2.54 mm 8PIN 排座 |
| 外壳 | CNC 铝合金 | 一体成型，阳极氧化，预留挂绳孔 |

本板 **没有** PCF85063 RTC，也 **没有** TF / microSD 卡槽。

## 与非 C 版 1.75 的差异

相对 [ESP32-S3-Touch-AMOLED-1.75](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-1.75)：

| 项目 | 1.75 | 1.75C |
| --- | --- | --- |
| Flash | 16 MB | **32 MB** |
| RTC（PCF85063） | 有 | 无 |
| TF / microSD | 有 | 无 |
| 扩展口 | 2.54 mm 8PIN 排座 | GPIO 焊盘 |
| 外壳 | 开发板形态 | CNC 铝合金电子吧唧外壳 |

## 软件中使用的主要引脚

以下来自本仓库 Arduino `pin_config.h` 与 ESP-IDF BSP，便于对照示例代码。原理图以 `Schematic/` 及官方文档为准。

| 功能 | GPIO | 说明 |
| --- | --- | --- |
| LCD QSPI D0–D3 | 4 / 5 / 6 / 7 | CO5300 数据 |
| LCD SCLK | 38 | QSPI 时钟 |
| LCD CS | 12 | 片选 |
| LCD RST | 1（IDF BSP）/ 2（Arduino） | 两套示例定义不一致，以所用框架为准 |
| I2C SDA / SCL | 15 / 14 | 触摸、电源、IMU 等 |
| 触摸 INT | 11 | CST9217 中断 |
| 触摸 RST | 2 | CST9217 复位 |
| I2S BCLK / LRCK / MCLK | 9 / 45 / 16 | 音频时钟 |
| I2S DIN / DOUT | 10 / 8 | ES7210 输入 / ES8311 输出 |
| 功放使能 | 46 | 扬声器功放 |

## 参考资料

- [中文文档](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-1.75C)
- [英文文档](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75C)
- [产品页](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.75C.htm)
- ESP-IDF BSP：`examples/ESP-IDF-v5.5/02_lvgl_demo_v9/components/esp32_s3_touch_amoled_1_75c/`
- Arduino 引脚：`examples/Arduino-v3.3.5/libraries/Mylibrary/pin_config.h`
