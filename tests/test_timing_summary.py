import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("summary", Path(__file__).resolve().parents[1] / "tools/timing_replay_summary.py")
summary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summary)
SHA = "89ac294d9be103c4112672f48fa16648f46501518eb4172ea5d22fbd494ad414"
LOG = "physical FFB disabled\nvisual replay complete"
ROWS = [{"kind": "sample", "missed_delta": 2}, {"kind": "final", "simulation_total": 11364,
         "presentations_total": 11000, "missed_delta": 3, "target_hz": 60, "display_hz": 180}]

class SummaryTests(unittest.TestCase):
    def test_deltas_not_nonexistent_total_and_success(self):
        result = summary.summarize(ROWS, LOG, 0, SHA)
        self.assertTrue(result["validated"])
        self.assertEqual(result["missed"], 5)

    def test_complete_engine_is_not_success_without_final_or_matching_frame(self):
        for rows, sha in (([], SHA), (ROWS, "bad")):
            result = summary.summarize(rows, LOG, 0, sha)
            self.assertTrue(result["engineComplete"])
            self.assertFalse(result["validated"])

    def test_failed_engine_and_force_initialization_rejected(self):
        for code, text in ((1, LOG), (0, LOG + "\n[fzero-ffb] active"), (0, "visual replay complete")):
            self.assertFalse(summary.summarize(ROWS, text, code, SHA)["validated"])

if __name__ == "__main__":
    unittest.main()
