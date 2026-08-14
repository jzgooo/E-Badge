# E-Badge（ESP32-S3-Touch-AMOLED-1.75C）

仓库：[jzgooo/esp32-s3-badge](https://github.com/jzgooo/esp32-s3-badge)

[English](README.md) | **中文**

微雪 ESP32-S3 1.75 寸圆形 AMOLED 触摸开发板（466×466，QSPI，双麦，电子吧唧形态）。本仓库在厂商工程样例之上：

- **`main`**：基础固件（稳定接口 + 可运行骨架）
- **其它分支**：基于 `main` 做二次开发（例如 Codex 额度屏）

约定见 [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)。

---

## 目录说明

| 路径 | 作用 |
| --- | --- |
| [`app/`](app/) | `main` 上的基础固件：接口在 `include/`，骨架实现在 `src/` |
| [`docs/PRD.md`](docs/PRD.md) | 产品需求（Codex 额度屏，在功能分支实现） |
| [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) | 分支约定：main 打底，其它分支二次开发 |
| [`examples/`](examples/) | 微雪 Arduino / ESP-IDF 样例，只读对照 |
| [`Firmware/`](Firmware/) | 出厂固件 bin |
| [`Schematic/`](Schematic/) | 原理图 PDF |
| [`HARDWARE.md`](HARDWARE.md) | 硬件规格 |

产品固件请用 ESP-IDF 5.5+ 在 `app/` 下编译，详见 [`app/README.md`](app/README.md)。二次开发请从 `main` 拉分支，不要直接在 `main` 上堆某一款产品的实现。

---

## 产品方向（功能分支实现）

圆屏 Codex 额度展示的需求在 [docs/PRD.md](docs/PRD.md)。请在从 `main` 拉出的分支上实现画面与 BLE 字段，基础开屏与接口留在 `main`。

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

1. 从 `main` 拉分支（不要在 `main` 上直接做某一款产品）
2. 优先改 `app/components/*/src/`，保持 `include/` 接口稳定
3. 只有接口或骨架需要成为新基础时，再 PR 回 `main`

详见 [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)。`examples/` 不要加业务。

---

## 问题与支持

- 本仓库：[Issues](https://github.com/jzgooo/esp32-s3-badge/issues)
- 开发板本身：可参考[微雪 Issues](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/issues)，或凭订单号联系微雪

---

## 许可证

Apache License，详见 `LICENSE`。

---

## 致谢

- 微雪的硬件与例程
- 乐鑫 ESP-IDF
- 开源贡献者
