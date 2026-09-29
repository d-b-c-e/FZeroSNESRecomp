import json
from argparse import Namespace
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from tools.fzero_replay_adapter import create_case, observe, raw_rows, requests, sha256


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
            arguments = Namespace(case=case_path, case_id="test-drive", drive=drive,
                                  state=state, config=config, video=video, rom=rom,
                                  msu_pack=pack, capture_exe=exe, source_revision="testrevision",
                                  capture_receipt=None)
            alternate = root / "diagnostics" / "alternate.state"
            alternate.write_bytes(b"other state")
            arguments.state = alternate
            with self.assertRaisesRegex(ValueError, ".state sibling"):
                create_case(arguments)
            arguments.state = state
            create_case(arguments)
            case = json.loads(case_path.read_text())
            self.assertTrue(case["provenance"]["dirty"])  # no clean build receipt
            config_copy = root / case["artifacts"]["config"]["path"]
            patch_copy = root / case["artifacts"]["msuPatch"]["path"]
            self.assertEqual(sha256(config_copy), sha256(config))
            self.assertEqual(sha256(patch_copy), sha256(patch))
            config.write_text("[Sound]\nMsu1Enabled=0\n")
            self.assertNotEqual(sha256(config_copy), sha256(config))

    def test_manifest_state_must_be_exact_loaded_sibling_before_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            drive = root / "drive.fzpt"
            drive.write_bytes(b"FZPT0001" + b"x" * 32 + (1).to_bytes(8, "little") + b"\x01" + b"\x00" * 12)
            loaded_sibling = root / "drive.fzpt.state"
            loaded_sibling.write_bytes(b"not the recorded state")
            pinned_state = root / "different.state"
            pinned_state.write_bytes(b"valid but different state")
            case_path = root / "case.json"
            case = {
                "schema": "dbce.wheel.replay-case", "version": 1,
                "caseId": "mismatched-state",
                "game": "fzero-snes-recomp", "adapter": "fzero-fzpt@1",
                "capability": "game-input-replay",
                "clock": {"domain": "emulated", "ticksPerSecond": 21477272},
                "source": {"path": drive.name, "sha256": sha256(drive), "format": "fzero-fzpt@1"},
                "initialState": {"path": pinned_state.name, "sha256": sha256(pinned_state),
                                 "format": "fzero-snapshot@1"},
                "artifacts": {},
                "provenance": {"sourceRevision": "testrevision", "dirty": True,
                               "executableSha256": "0" * 64},
            }
            case_path.write_text(json.dumps(case))

            def validate(path):
                record = json.loads(path.read_text())
                self.assertEqual(sha256(root / record["source"]["path"]), record["source"]["sha256"])
                self.assertEqual(sha256(root / record["initialState"]["path"]),
                                 record["initialState"]["sha256"])
                return {"caseSha256": sha256(path)}

            toolkit = SimpleNamespace(validate_case=validate, decode=lambda data: json.loads(data))
            args = Namespace(case=case_path, toolkit=root, runner=root / "headless.exe",
                             rom=root / "rom.sfc", output=root / "observation.jsonl", strength=12)
            with mock.patch("tools.fzero_replay_adapter.load_toolkit", return_value=toolkit), \
                 mock.patch("tools.fzero_replay_adapter.subprocess.run") as launch:
                with self.assertRaisesRegex(ValueError, "not the .state sibling"):
                    observe(args)
                launch.assert_not_called()
            self.assertFalse(args.output.exists())

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
