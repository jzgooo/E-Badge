#!/usr/bin/env python3
"""无需蓝牙硬件的桥接器组件测试。"""

import unittest
from unittest.mock import patch

from codex_usage_bridge import build_snapshot


class BridgeSnapshotTests(unittest.TestCase):
    @patch("codex_usage_bridge.time.time", return_value=1_780_000_000)
    def test_creates_quota_snapshot(self, _clock):
        snapshot = build_snapshot(None, 72, "five_hour")
        self.assertEqual(snapshot["default_card"], "quota")
        self.assertEqual(snapshot["cards"][0]["label"], "72%")
        self.assertEqual(snapshot["cards"][0]["caption"], "代码助手 5小时")

    @patch("codex_usage_bridge.time.time", return_value=1_780_000_000)
    def test_preserves_other_cards_and_default(self, _clock):
        previous = {
            "version": 1,
            "default_card": "build",
            "cards": [
                {"id": "quota", "percent": 10, "label": "10%", "caption": "旧额度", "updated_at": 1_770_000_000, "ttl_sec": 86400, "severity": "normal"},
                {"id": "build", "percent": 255, "label": "成功", "caption": "main", "updated_at": 1_770_000_000, "ttl_sec": 7200, "severity": "normal"},
            ],
        }
        snapshot = build_snapshot(previous, 88, "weekly")
        self.assertEqual(snapshot["default_card"], "build")
        self.assertEqual([card["id"] for card in snapshot["cards"]], ["quota", "build"])
        self.assertEqual(snapshot["cards"][0]["caption"], "代码助手 本周")


if __name__ == "__main__":
    unittest.main()
