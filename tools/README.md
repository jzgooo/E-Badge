# 开发者桌面状态徽章推送工具

此目录提供 Windows 上的 BLE 命令行工具。它只把已生成的展示 JSON 写入徽章；不读取或保存任何账号、浏览器登录信息或接口密钥。

```powershell
python -m pip install -r tools/requirements.txt
python tools/badge_push.py scan
python tools/badge_push.py example
python tools/badge_push.py push tools/sample_dashboard.json
python tools/badge_push.py read
```

也可将 JSON 从标准输入传入：

```powershell
Get-Content .\status.json -Raw | python tools/badge_push.py push -
```

## 代码助手额度浏览器连接器

若希望把 ChatGPT 用量页中**可见的**代码助手剩余百分比自动同步到徽章，可使用 [`codex_usage_extension/`](codex_usage_extension/) 与本机转发器。该方案由用户在自己的 Chrome/Edge 中登录和安装扩展；扩展不读取 Cookie、令牌或网络请求，只发送一个百分比和时间窗口给监听在 `127.0.0.1` 的 Python 服务。完整安装和隐私说明见 [扩展说明](codex_usage_extension/README.md)。

设备服务为 `0xFF00`：旧额度 JSON 位于 `0xFF01`，完整仪表盘快照位于 `0xFF02`。新协议最多接受 4 张卡片，编码后的 JSON 不得超过 1023 字节。

卡片 ID 只能是 `quota`、`build`、`focus`、`schedule`。`label` 最多 16 个 UTF-8 字节，`caption` 最多 24 个 UTF-8 字节；`focus` 与 `schedule` 必须携带 `target_at`。完整字段见 [`../docs/开发者桌面状态徽章_PRD.md`](../docs/开发者桌面状态徽章_PRD.md)。

从开机、BLE 推送到浏览器额度同步的完整操作和排障见 [`../docs/使用教程.md`](../docs/使用教程.md)。
