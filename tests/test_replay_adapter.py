import json
from argparse import Namespace
import tempfile
import unittest
from pathlib import Path

from tools.fzero_replay_adapter import create_case, raw_rows, requests, sha256


class ReplayAdapterTests(unittest.TestCase):
    def test_case_snapshots_config_and_patch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "diagnostics").mkdir()
            pack = root / "music" / "pack"
            pack.mkdir(parents=True)
            patch = pack / "f-zero_msu1_stock.ips"
            patch.write_bytes(b"PATCHEOF")
            drive = root / "diagnostics" / "drive.fzpt"
            drive.write_bytes(b"FZPT0001" + b"x" * 32 + (1).to_bytes(8, "little") + b"\x01" + b"\x00" * 12)
            state = root / "diagnostics" / "drive.fzpt.state"
            state.write_bytes(b"state")
            config = root / "config.ini"
            config.write_text("[Sound]\nMsu1Enabled=1\nMsu1Dir=music/pack\n[ForceFeedback]\nStrength=12\n")
            video = root / "fzero-video.ini"
            video.write_text("[FZeroVideo]\nAspect=Fit\nTripleScreen=1\nBSDeluxe=0\n")
            rom = root / "rom.sfc"
            rom.write_bytes(b"rom")
            exe = root / "game.exe"
            exe.write_bytes(b"exe")
            case_path = root / "case.json"
            create_case(Namespace(case=case_path, case_id="test-drive", drive=drive,
                                  state=state, config=config, video=video, rom=rom,
                                  msu_pack=pack, capture_exe=exe, source_revision="testrevision"))
            case = json.loads(case_path.read_text())
            config_copy = root / case["artifacts"]["config"]["path"]
            patch_copy = root / case["artifacts"]["msuPatch"]["path"]
            self.assertEqual(sha256(config_copy), sha256(config))
            self.assertEqual(sha256(patch_copy), sha256(patch))
            config.write_text("[Sound]\nMsu1Enabled=0\n")
            self.assertNotEqual(sha256(config_copy), sha256(config))

    def test_complete_model_edges_and_normalization(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "model.raw"
            path.write_bytes(
                b"FZFFB1\t12\n"
                b"0\t357366\t1\t660\t1200\t480\t480\t42000\t0\n"
                b"1\t714732\t1\t660\t1200\t480\t480\t42000\t1\n"
                b"2\t1072098\t0\t0\t0\t0\t0\t0\t0\n"
                b"complete\t3\n"
            )
            rows = list(raw_rows(path, 3, 12))
            events = list(requests(rows, 12))
            self.assertEqual(len(events), 14)  # four requests/frame, one impact, one stop-all
            self.assertEqual([event["sequence"] for event in events], list(range(14)))
            self.assertEqual(events[1]["magnitude"], 0.12)
            self.assertEqual(events[3]["frequencyHz"], 42)
            self.assertEqual(events[8]["effect"], "impact")
            self.assertEqual(events[8]["operation"], "start")
            self.assertEqual(events[8]["magnitude"], 0.12)
            self.assertEqual(events[8]["durationMs"], 140)
            self.assertEqual(events[-2]["frequencyHz"], 1)  # consumer clamps idle sine
            self.assertEqual(events[-1]["operation"], "stop_all")

    def test_incomplete_or_misaligned_model_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "model.raw"
            for content in (
                b"FZFFB1\t12\n0\t100\t1\t0\t0\t0\t0\t0\t0\n",
                b"FZFFB1\t12\n1\t100\t1\t0\t0\t0\t0\t0\t0\ncomplete\t1\n",
                b"FZFFB1\t11\n0\t100\t1\t0\t0\t0\t0\t0\t0\ncomplete\t1\n",
            ):
                path.write_bytes(content)
                with self.assertRaises(ValueError):
                    list(raw_rows(path, 1, 12))


if __name__ == "__main__":
    unittest.main()
