import copy
import json
import unittest
from pathlib import Path

import badge_push


class SnapshotValidationTests(unittest.TestCase):
    def setUp(self):
        self.snapshot = json.loads(Path(__file__).with_name("sample_dashboard.json").read_text(encoding="utf-8"))

    def test_sample_is_valid_and_bounded(self):
        snapshot = badge_push.validate_snapshot(self.snapshot)
        payload = json.dumps(snapshot, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.assertLessEqual(len(payload), badge_push.MAX_PAYLOAD)

    def test_rejects_duplicate_card_id(self):
        snapshot = copy.deepcopy(self.snapshot)
        snapshot["cards"][1]["id"] = "quota"
        with self.assertRaises(ValueError):
            badge_push.validate_snapshot(snapshot)

    def test_rejects_oversized_utf8_caption(self):
        snapshot = copy.deepcopy(self.snapshot)
        snapshot["cards"][0]["caption"] = "中" * 9
        with self.assertRaises(ValueError):
            badge_push.validate_snapshot(snapshot)

    def test_requires_time_target_for_schedule(self):
        snapshot = copy.deepcopy(self.snapshot)
        del snapshot["cards"][3]["target_at"]
        with self.assertRaises(ValueError):
            badge_push.validate_snapshot(snapshot)


if __name__ == "__main__":
    unittest.main()
