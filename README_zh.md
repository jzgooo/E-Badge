# E-Badge（ESP32-S3-Touch-AMOLED-1.75C）

[English](README.md) | **中文**

微雪 ESP32-S3 1.75 寸圆形 AMOLED 触摸开发板（466×466，QSPI，双麦，电子吧唧形态）。本仓库在厂商工程样例之上，产品固件只做 **Codex 额度展示**。

---

## 目录说明

| 路径 | 作用 |
| --- | --- |
| [`app/`](app/) | 产品固件，业务代码从这里开始 |
| [`docs/PRD.md`](docs/PRD.md) | 产品需求（仅 Codex 额度屏） |
| [`examples/`](examples/) | 微雪 Arduino / ESP-IDF 样例，只读对照 |
| [`Firmware/`](Firmware/) | 出厂固件 bin |
| [`Schematic/`](Schematic/) | 原理图 PDF |
| [`HARDWARE.md`](HARDWARE.md) | 硬件规格 |

产品固件请用 ESP-IDF 5.5+ 在 `app/` 下编译，详见 [`app/README.md`](app/README.md)。

---

## 产品（Codex 额度屏）

圆屏只显示剩余额度（环 + 大数字）。数字由电脑经 BLE 推送，设备不登录 OpenAI。

```bash
cd app
idf.py set-target esp32s3
idf.py build
idf.py -p <串口> flash monitor
```

---

## 硬件与配置

规格见 [HARDWARE.md](HARDWARE.md)。更完整的板级说明见[微雪文档](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-1.75C)。

本板 **没有** TF / microSD 卡槽，也没有 GPS。存储为 32 MB Flash + 8 MB PSRAM（堆）。

---

## 参与贡献

1. Fork 本仓库
2. 新建分支开发
3. 提交说明清楚的 commit
4. 发起 Pull Request

产品功能请改 `app/`，不要在 `examples/` 里加业务。

---

## 问题与支持

- 本仓库：[Issues](https://github.com/jzgooo/E-Badge/issues)
- 开发板本身：可参考[微雪 Issues](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/issues)，或凭订单号联系微雪

---

## 许可证

Apache License，详见 `LICENSE`。

---

## 致谢

- 微雪的硬件与例程
- 乐鑫 ESP-IDF
- 开源贡献者
