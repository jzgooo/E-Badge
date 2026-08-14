# E-Badge 基础固件

`main` 上的 `app/` 是**基础代码**：可编译的骨架 + 稳定接口（各组件 `include/`）。具体产品在从 `main` 拉出的分支上改 `src/` 做二次开发，约定见 [docs/DEVELOPMENT.md](../docs/DEVELOPMENT.md)。

产品方向见 [docs/PRD.md](../docs/PRD.md)。厂商 `examples/` 只读对照，不要在那边加业务。

## 目录

```
app/
├── main/                 # 组装入口：只调接口，不写业务细节
└── components/
    ├── board/            # 接口 board.h；实现：NVS、开屏
    ├── ui/               # 接口 ui.h；实现：画面（二次开发主改这里）
    ├── badge/            # 接口 badge.h；实现：展示内容
    ├── sensors/          # 接口 sensors.h；骨架
    ├── audio/            # 接口 audio.h；骨架
    └── ble/              # 接口 ble.h；实现：广播 / GATT
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
