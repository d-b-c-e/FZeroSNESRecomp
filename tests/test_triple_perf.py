import unittest

from tools.summarize_triple_perf import summarize


def sample(elapsed, missed, scene, draw_total, draw_calls,
           projection_total=0, projection_calls=0):
    return {
        "kind": "sample", "scene": scene, "simulation_delta": 120,
        "presentations_delta": draw_calls, "missed_delta": missed,
        "elapsed_ms": elapsed, "target_hz": 59.95,
        "frame_interval_p99_ms": 17.5 + missed,
        "stages": {
            "draw_submit": {"total_ms": draw_total, "calls": draw_calls},
            "triple_projection": {"total_ms": projection_total,
                                  "calls": projection_calls},
        },
    }


class TriplePerfTests(unittest.TestCase):
    def test_combined_weighted_means_and_race_filter(self):
        rows = [sample(2000, 20, 2, 400, 40),
                sample(4000, 1, 2, 600, 100),
                sample(6000, 99, 1, 5, 1)]
        result = summarize(rows)
        self.assertFalse(result["split"])
        self.assertEqual(result["missed"], 21)
        self.assertEqual(result["presentations"], 140)
        self.assertAlmostEqual(result["stage_means"]["draw_submit"], 1000 / 140)
        self.assertEqual(result["worst"][0][1], 20)

    def test_split_projection_is_separate(self):
        rows = [sample(2000, 0, 2, 5, 10, 80, 10)]
        result = summarize(rows)
        self.assertTrue(result["split"])
        self.assertAlmostEqual(result["stage_means"]["draw_submit"], 0.5)
        self.assertAlmostEqual(result["stage_means"]["triple_projection"], 8)

    def test_requires_matching_samples(self):
        with self.assertRaisesRegex(ValueError, "no scene 2 samples"):
            summarize([sample(2000, 0, 1, 5, 10)])


if __name__ == "__main__":
    unittest.main()
