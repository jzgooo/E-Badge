#!/usr/bin/env python3
"""Windows BLE command-line tool for the Developer Desktop Badge.

The tool accepts a complete dashboard snapshot from a file or stdin. It never
reads, stores, or prints credentials; external data-source scripts own that work.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
import time
from pathlib import Path
from typing import Any

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover - exercised by real users before install
    BleakClient = None
    BleakScanner = None


DEVICE_NAME = "Codex"
SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb"
LEGACY_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"
DASHBOARD_UUID = "0000ff02-0000-1000-8000-00805f9b34fb"
MAX_PAYLOAD = 1023
CARD_IDS = {"quota", "build", "focus", "schedule"}
SEVERITIES = {"normal", "near", "critical"}


def require_bleak() -> None:
    if BleakClient is None or BleakScanner is None:
        raise RuntimeError("缺少 bleak。请执行: python -m pip install -r tools/requirements.txt")


def validate_snapshot(snapshot: Any) -> dict[str, Any]:
    if not isinstance(snapshot, dict):
        raise ValueError("顶层必须是 JSON 对象")
    if snapshot.get("version") != 1:
        raise ValueError("version 必须为 1")
    cards = snapshot.get("cards")
    if not isinstance(cards, list) or not 1 <= len(cards) <= 4:
        raise ValueError("cards 必须包含 1 到 4 张卡片")
    if snapshot.get("default_card") not in CARD_IDS:
        raise ValueError("default_card 必须是 quota/build/focus/schedule")
    seen: set[str] = set()
    for card in cards:
        if not isinstance(card, dict):
            raise ValueError("每张卡片必须是对象")
        card_id = card.get("id")
        if card_id not in CARD_IDS or card_id in seen:
            raise ValueError("卡片 id 必须为四种预定义值且不能重复")
        seen.add(card_id)
        percent = card.get("percent")
        if not isinstance(percent, int) or not (0 <= percent <= 100 or percent == 255):
            raise ValueError(f"{card_id}.percent 必须为 0-100 或 255")
        if not isinstance(card.get("label"), str) or not card["label"]:
            raise ValueError(f"{card_id}.label 必须是非空文本")
        if len(card["label"].encode("utf-8")) > 16:
            raise ValueError(f"{card_id}.label 最多 16 个 UTF-8 字节")
        caption = card.get("caption", "")
        if not isinstance(caption, str) or len(caption.encode("utf-8")) > 24:
            raise ValueError(f"{card_id}.caption 最多 24 个 UTF-8 字节")
        if not isinstance(card.get("updated_at"), int) or card["updated_at"] <= 0:
            raise ValueError(f"{card_id}.updated_at 必须是正整数 Unix 秒")
        if not isinstance(card.get("ttl_sec"), int) or not 0 < card["ttl_sec"] <= 604800:
            raise ValueError(f"{card_id}.ttl_sec 必须在 1..604800")
        if card.get("severity") not in SEVERITIES:
            raise ValueError(f"{card_id}.severity 无效")
        if card_id in {"focus", "schedule"}:
            target = card.get("target_at")
            if not isinstance(target, int) or target <= card["updated_at"]:
                raise ValueError(f"{card_id}.target_at 必须晚于 updated_at")
    if snapshot["default_card"] not in seen:
        raise ValueError("default_card 必须存在于 cards")
    return snapshot


def load_snapshot(path: str) -> tuple[dict[str, Any], bytes]:
    raw = sys.stdin.read() if path == "-" else Path(path).read_text(encoding="utf-8")
    snapshot = validate_snapshot(json.loads(raw))
    payload = json.dumps(snapshot, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"编码后的 JSON 为 {len(payload)} 字节，超过设备 {MAX_PAYLOAD} 字节限制")
    return snapshot, payload


async def resolve_address(address: str | None, name: str, timeout: float) -> str:
    if address:
        return address
    require_bleak()
    devices = await BleakScanner.discover(timeout=timeout)
    for device in devices:
        if device.name == name:
            return device.address
    raise RuntimeError(f"未找到名为 {name!r} 的设备；可先运行 scan，或使用 --address")


async def cmd_scan(args: argparse.Namespace) -> int:
    require_bleak()
    devices = await BleakScanner.discover(timeout=args.timeout)
    found = False
    for device in devices:
        if not args.name or device.name == args.name:
            print(f"{device.address}\t{device.name or '(unnamed)'}")
            found = True
    return 0 if found else 1


async def cmd_push(args: argparse.Namespace) -> int:
    _, payload = load_snapshot(args.input)
    address = await resolve_address(args.address, args.name, args.timeout)
    async with BleakClient(address, timeout=args.timeout) as client:
        if not client.is_connected:
            raise RuntimeError("BLE 连接失败")
        await client.write_gatt_char(DASHBOARD_UUID, payload, response=True)
    print(f"已推送 {len(payload)} 字节到 {address}")
    return 0


async def cmd_read(args: argparse.Namespace) -> int:
    address = await resolve_address(args.address, args.name, args.timeout)
    uuid = LEGACY_UUID if args.legacy else DASHBOARD_UUID
    async with BleakClient(address, timeout=args.timeout) as client:
        value = await client.read_gatt_char(uuid)
    print(bytes(value).decode("utf-8"))
    return 0


def cmd_example(_: argparse.Namespace) -> int:
    now = int(time.time())
    sample = {
        "version": 1,
        "default_card": "quota",
        "cards": [
            {"id": "quota", "percent": 72, "label": "72%", "caption": "代码助手 5小时", "updated_at": now, "ttl_sec": 86400, "severity": "normal"},
            {"id": "build", "percent": 255, "label": "失败", "caption": "main #183", "updated_at": now, "ttl_sec": 7200, "severity": "critical"},
            {"id": "focus", "percent": 60, "label": "专注", "caption": "深度工作", "updated_at": now, "ttl_sec": 3600, "target_at": now + 1500, "severity": "normal"},
            {"id": "schedule", "percent": 255, "label": "会议", "caption": "项目同步", "updated_at": now, "ttl_sec": 900, "target_at": now + 480, "severity": "near"},
        ],
    }
    print(json.dumps(sample, ensure_ascii=False, indent=2))
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="开发者桌面状态徽章 BLE 推送工具")
    sub = parser.add_subparsers(dest="command", required=True)
    scan = sub.add_parser("scan", help="扫描设备")
    scan.add_argument("--name", default=DEVICE_NAME)
    scan.add_argument("--timeout", type=float, default=8.0)
    scan.set_defaults(handler=cmd_scan)
    for command, help_text in (("push", "推送完整仪表盘快照"), ("read", "读取设备状态")):
        item = sub.add_parser(command, help=help_text)
        item.add_argument("--address", help="BLE 地址；未指定时按设备名扫描")
        item.add_argument("--name", default=DEVICE_NAME)
        item.add_argument("--timeout", type=float, default=12.0)
        if command == "push":
            item.add_argument("input", help="JSON 文件路径；- 表示标准输入")
            item.set_defaults(handler=cmd_push)
        else:
            item.add_argument("--legacy", action="store_true", help="读取旧额度特征 0xFF01")
            item.set_defaults(handler=cmd_read)
    example = sub.add_parser("example", help="输出四卡片示例 JSON")
    example.set_defaults(handler=cmd_example)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        result = args.handler(args)
        return asyncio.run(result) if asyncio.iscoroutine(result) else result
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as exc:
        print(f"错误: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
