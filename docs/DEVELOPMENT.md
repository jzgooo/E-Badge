# 开发约定：main 是基础，其它分支做二次开发

## 原则

- **`main`**：基础代码。保证能编译、能点亮，并提供稳定的 C 接口（各组件 `include/*.h`）。
- **其它分支**：从 `main` 拉出，只改实现（主要是 `src/`），完成具体产品（例如 Codex 额度屏）。
- **不要**在二次开发分支上随意改对外接口；接口要变，先合回 `main`，再让各分支来 rebase / merge。

```
main          接口 + 骨架实现（board / ui / badge / ble / sensors / audio）
  │
  ├─ feature/…   额度环、推送工具、电源细节…
  └─ feature/…   其它二次开发
```

厂商 `examples/` 不是基础代码，只作对照，二次开发不要在那里加功能。

## 基础接口（main 维护）

启动顺序固定在 `app/main/app_main.c`：

`board_init` → `badge_init` → `ui_start` → `sensors_start` → `audio_start` → `ble_start`

| 组件 | 头文件 | 二次开发时通常改哪里 |
| --- | --- | --- |
| board | `app/components/board/include/board.h` | `src/board.c`（电源、按键） |
| badge | `app/components/badge/include/badge.h` | `src/`（NVS、额度字段） |
| ui | `app/components/ui/include/ui.h` | `src/screen_*.c`（画面） |
| ble | `app/components/ble/include/ble.h` | `src/ble.c`（GATT JSON） |
| sensors | `app/components/sensors/include/sensors.h` | `src/sensors.c` |
| audio | `app/components/audio/include/audio.h` | `src/audio.c` |

`include/` 里的函数签名是契约：其它模块只通过这些头文件互相调用。新增对外函数要先上 `main`。

## 如何开二次开发分支

```bash
git fetch origin
git checkout -b <你的分支> origin/main
```

本仓库云端分支名形如 `cursor/<简短英文>-5533`。

建议：

1. 先在本机 `cd app && idf.py build`，确认基础能编过。
2. 实现只动对应组件的 `src/`，以及必要时的 `tools/`。
3. 产品范围见 [PRD.md](PRD.md)；当前产品方向是 Codex 额度展示。
4. 定期 `git merge origin/main` 或 rebase，吃到接口更新。
5. 只有「接口/骨架也要进基础」时，才把改动 PR 进 `main`。纯产品实现可以长期停在功能分支。

## 什么该进 main

**应该进**

- 新的稳定 API（头文件）
- 所有分支都需要的板级修复（点亮、NVS、BSP 依赖）
- 文档、忽略规则、能让骨架继续可编译的改动

**不该进（放功能分支）**

- 某一款产品的 UI 视觉、GATT 字段、配套 `tools/` 脚本
- 未稳定的实验、仅某次活动用的文案

## 参考

- [PRD.md](PRD.md) 当前产品需求
- [../HARDWARE.md](../HARDWARE.md) 硬件规格
- [../app/README.md](../app/README.md) 编译
