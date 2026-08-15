# E-Badge / Codex 固件

`main` 上的 `app/` 是**基础代码**：可编译的骨架 + 稳定接口（各组件 `include/`）。本分支（Codex 额度屏 M1）在 `src/` 做二次开发，约定见 [docs/DEVELOPMENT.md](../docs/DEVELOPMENT.md)。

产品方向见 [docs/PRD.md](../docs/PRD.md)。厂商 `examples/` 只读对照，不要在那边加业务。

## 本分支 M1+M2 行为

- 广播名 **`Codex`**
- GATT：服务 `0xFF00`，特征 `0xFF01`（JSON 读写四字段）
- 主界面：剩余额度环；Clear quota 清 NVS
- 息屏：设置 5/10/30 秒（NVS）；点按亮屏；PWR 短按亮屏/进设置、长按关机
- 低电 &lt;15% 降亮度；充电时顶部电量点闪烁；过期灰环会定时刷新
- 验收示例（nRF Connect 写入）：

```json
{"remain_percent":35,"remain_label":"35%","quota_caption":"Codex 5h","quota_updated_at":1730000000}
```

## 目录

```
app/
├── main/                 # 组装入口：只调接口，不写业务细节
└── components/
    ├── board/            # 接口 board.h；实现：NVS、开屏、亮度
    ├── ui/               # 接口 ui.h；实现：圆环 / 设置
    ├── badge/            # 接口 badge.h；实现：四字段 + NVS
    ├── sensors/          # 接口 sensors.h；骨架
    ├── audio/            # 接口 audio.h；骨架
    └── ble/              # 接口 ble.h；实现：广播 / GATT JSON
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
