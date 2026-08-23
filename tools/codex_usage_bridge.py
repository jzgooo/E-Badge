#!/usr/bin/env python3
"""本机 Codex 用量桥接器：接收浏览器扩展的可见百分比并推送到 BLE 徽章。

服务只监听 127.0.0.1，不读取浏览器 Cookie、令牌或网页正文，也不持久化
接收到的用量。可选的本机口令用于限制哪些本地扩展实例能够调用服务。
"""

from __future__ import annotations

import argparse
import asyncio
import json
import secrets
import sys
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

from badge_push import DASHBOARD_UUID, MAX_PAYLOAD, BleakClient, require_bleak, resolve_address, validate_snapshot


WINDOW_CAPTIONS = {
    "five_hour": "代码助手 5小时",
    "weekly": "代码助手 本周",
    "unknown": "代码助手额度",
}


def build_snapshot(previous: dict[str, Any] | None, percent: int, window: str) -> dict[str, Any]:
    """Return a full dashboard snapshot while preserving non-quota cards."""
    now = int(time.time())
    caption = WINDOW_CAPTIONS.get(window, WINDOW_CAPTIONS["unknown"])
    quota = {
        "id": "quota",
        "percent": percent,
        "label": f"{percent}%",
        "caption": caption,
        "updated_at": now,
        "ttl_sec": 86400,
        "severity": "normal",
    }
    cards = []
    if previous:
        cards = [card for card in previous.get("cards", []) if card.get("id") != "quota"]
    cards.insert(0, quota)
    snapshot = {
        "version": 1,
        "default_card": previous.get("default_card", "quota") if previous else "quota",
        "cards": cards,
    }
    if snapshot["default_card"] not in {card["id"] for card in cards}:
        snapshot["default_card"] = "quota"
    return validate_snapshot(snapshot)


async def push_usage(address: str | None, name: str, timeout: float, percent: int, window: str) -> str:
    require_bleak()
    target = await resolve_address(address, name, timeout)
    async with BleakClient(target, timeout=timeout) as client:
        previous: dict[str, Any] | None = None
        try:
            raw = bytes(await client.read_gatt_char(DASHBOARD_UUID)).decode("utf-8")
            parsed = json.loads(raw)
            if isinstance(parsed, dict) and parsed.get("cards"):
                previous = validate_snapshot(parsed)
        except Exception:
            # An empty/new device may not have a dashboard snapshot yet.  It is
            # safe to create a quota-only snapshot in that case.
            previous = None
        snapshot = build_snapshot(previous, percent, window)
        payload = json.dumps(snapshot, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if len(payload) > MAX_PAYLOAD:
            raise ValueError("现有仪表盘数据过长，无法在不覆盖其他卡片的情况下更新额度")
        await client.write_gatt_char(DASHBOARD_UUID, payload, response=True)
    return target


class BridgeServer(ThreadingHTTPServer):
    """Loopback-only HTTP server configuration."""

    token: str | None
    address: str | None
    name: str
    timeout: float


class RequestHandler(BaseHTTPRequestHandler):
    server: BridgeServer

    def log_message(self, _format: str, *_args: object) -> None:
        # Do not log request paths or bodies; usage data is intentionally ephemeral.
        return

    def _reply(self, status: HTTPStatus, message: str) -> None:
        body = json.dumps({"ok": status < HTTPStatus.BAD_REQUEST, "message": message}, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self) -> None:  # noqa: N802
        self.send_response(HTTPStatus.NO_CONTENT)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-Badge-Token")
        self.end_headers()

    def do_POST(self) -> None:  # noqa: N802
        if self.path != "/v1/codex-usage":
            self._reply(HTTPStatus.NOT_FOUND, "地址不存在")
            return
        if self.server.token and not secrets.compare_digest(self.headers.get("X-Badge-Token", ""), self.server.token):
            self._reply(HTTPStatus.UNAUTHORIZED, "本机口令不正确")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 512:
                raise ValueError("请求长度无效")
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            percent = payload.get("percent")
            window = payload.get("window", "unknown")
            if not isinstance(percent, int) or not 0 <= percent <= 100:
                raise ValueError("percent 必须是 0 到 100 的整数")
            if window not in WINDOW_CAPTIONS:
                raise ValueError("window 无效")
            target = asyncio.run(push_usage(self.server.address, self.server.name, self.server.timeout, percent, window))
        except Exception as exc:  # BLE backends raise library-specific connection errors.
            print(f"额度推送失败: {exc}", file=sys.stderr)
            self._reply(HTTPStatus.BAD_REQUEST, f"推送失败：{exc}")
            return
        self._reply(HTTPStatus.OK, f"已推送到 {target}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Codex 用量浏览器扩展的本机 BLE 转发器")
    parser.add_argument("--address", help="BLE 地址；未指定时按设备名扫描")
    parser.add_argument("--name", default="Codex", help="设备广播名")
    parser.add_argument("--timeout", type=float, default=12.0, help="BLE 操作超时秒数")
    parser.add_argument("--port", type=int, default=8765, help="仅本机监听的端口")
    parser.add_argument("--token", help="可选本机口令；需与扩展设置一致")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 1 <= args.port <= 65535:
        print("错误: --port 必须为 1 到 65535", file=sys.stderr)
        return 2
    server = BridgeServer(("127.0.0.1", args.port), RequestHandler)
    server.token = args.token
    server.address = args.address
    server.name = args.name
    server.timeout = args.timeout
    protection = "已启用本机口令" if args.token else "未设置本机口令"
    print(f"本机转发器已启动：http://127.0.0.1:{args.port}/v1/codex-usage（{protection}）")
    print("按 Ctrl+C 停止；不会保存接收到的用量或浏览器凭证。")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n已停止。")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
