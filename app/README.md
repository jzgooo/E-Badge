# E-Badge 产品固件

业务代码从这里开始。产品范围见 [docs/PRD.md](../docs/PRD.md)（仅 Codex 额度展示）。`examples/` 里的 Arduino / ESP-IDF 工程是厂商样例，只作对照，不要在那边加产品功能。

## 目录

```
app/
├── main/                 # 组装入口：初始化顺序，不写具体业务
└── components/
    ├── board/            # 板级：NVS、显示、电源启动
    ├── ui/               # 界面：表盘 / 徽章页 / 设置
    ├── badge/            # 核心业务：徽章状态与展示内容
    ├── sensors/          # 姿态等传感器（骨架）
    ├── audio/            # 麦 / 喇叭（骨架）
    └── ble/              # BLE 广播与 GATT
```

LVGL、官方 BSP（`waveshare/esp32_s3_touch_amoled_1_75c`）由 Component Manager 拉取，不要拷进 git。

## 编译

需要 ESP-IDF 5.5+。

```bash
cd app
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```
