#!/usr/bin/env python3

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL_PATH = ROOT / "tools" / "issue_model.py"
SPEC = importlib.util.spec_from_file_location("issue_model", MODEL_PATH)
issue_model = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(issue_model)


class IssueModelTest(unittest.TestCase):
    def test_mix_parser_accepts_reordered_tokens(self):
        mix = issue_model.parse_mix("5L + 4F + 1S")
        self.assertEqual(mix.compute, 4.0)
        self.assertEqual(mix.load, 5.0)
        self.assertEqual(mix.store, 1.0)

    def test_mix_parser_rejects_duplicate_components(self):
        with self.assertRaisesRegex(ValueError, "more than once"):
            issue_model.parse_mix("1F+2C")

    def test_f1_l2_hits_load_ceiling(self):
        report = issue_model.evaluate(issue_model.parse_mix("1F+2L"))
        self.assertAlmostEqual(
            report["predicted"]["cycles_per_group"], 2.0 / 3.0
        )
        self.assertAlmostEqual(report["result"]["compute_ipc"], 1.5)
        self.assertAlmostEqual(report["result"]["load_ipc"], 3.0)
        self.assertAlmostEqual(report["result"]["total_ipc"], 4.5)
        self.assertAlmostEqual(report["efficiency"]["compute"], 0.375)
        self.assertAlmostEqual(report["efficiency"]["memory"], 1.0)
        self.assertEqual(report["predicted"]["bottlenecks"], ["load"])

    def test_f4_l5_hits_measured_shared_ceiling(self):
        report = issue_model.evaluate(issue_model.parse_mix("5L+4F"))
        self.assertAlmostEqual(report["predicted"]["cycles_per_group"], 1.8)
        self.assertAlmostEqual(report["result"]["compute_ipc"], 20.0 / 9.0)
        self.assertAlmostEqual(report["result"]["load_ipc"], 25.0 / 9.0)
        self.assertAlmostEqual(report["result"]["total_ipc"], 5.0)
        self.assertEqual(
            report["predicted"]["bottlenecks"], ["compute_load"]
        )

    def test_store_profile_uses_store_and_shared_pipeline_limits(self):
        pure_store = issue_model.evaluate(issue_model.parse_mix("2S"))
        self.assertAlmostEqual(
            pure_store["predicted"]["cycles_per_group"], 1.0
        )
        self.assertAlmostEqual(pure_store["result"]["store_ipc"], 2.0)

        mixed = issue_model.evaluate(issue_model.parse_mix("4F+3L+2S"))
        self.assertAlmostEqual(mixed["predicted"]["cycles_per_group"], 1.5)
        self.assertAlmostEqual(mixed["result"]["compute_ipc"], 8.0 / 3.0)
        self.assertAlmostEqual(mixed["result"]["load_ipc"], 2.0)
        self.assertAlmostEqual(mixed["result"]["store_ipc"], 4.0 / 3.0)
        self.assertEqual(
            mixed["predicted"]["bottlenecks"], ["compute_store"]
        )

    def test_capacity_override_changes_result(self):
        profile = issue_model.with_overrides(
            issue_model.DEFAULT_PROFILE, {"store": 1.0}
        )
        report = issue_model.evaluate(issue_model.parse_mix("2S"), profile)
        self.assertAlmostEqual(report["predicted"]["cycles_per_group"], 2.0)
        self.assertAlmostEqual(report["result"]["store_ipc"], 1.0)

    def test_observed_cycles_report_model_attainment(self):
        report = issue_model.evaluate(
            issue_model.parse_mix("4F+5L"),
            observed_cycles_per_group=1.812816,
        )
        self.assertEqual(report["mode"], "observed")
        self.assertAlmostEqual(
            report["result"]["load_ipc"], 5.0 / 1.812816
        )
        self.assertAlmostEqual(
            report["efficiency"]["model_attainment"], 1.8 / 1.812816
        )

    def test_measured_load_compute_points_are_within_two_percent(self):
        measured_cycles = {
            "0F+3L": 1.0016,
            "1F+2L": 0.668216,
            "2F+3L": 1.0096,
            "3F+3L": 1.2126,
            "4F+3L": 1.4171,
            "4F+5L": 1.812816,
            "2S": 1.001528,
            "3L+1S": 1.005510,
            "3L+2S": 1.252341,
            "4F+1S": 1.251540,
            "4F+3L+1S": 1.402215,
            "4F+3L+2S": 1.507225,
        }
        for mix_text, observed_cycles in measured_cycles.items():
            with self.subTest(mix=mix_text):
                report = issue_model.evaluate(
                    issue_model.parse_mix(mix_text),
                    observed_cycles_per_group=observed_cycles,
                )
                attainment = report["efficiency"]["model_attainment"]
                self.assertGreaterEqual(attainment, 0.98)
                self.assertLessEqual(attainment, 1.01)

    def test_v1_profile_uses_measured_sve256_limits(self):
        profile = issue_model.PROFILES["neoverse-v1-sve256-fmla"]
        report = issue_model.evaluate(
            issue_model.parse_mix("4F+3L+2S"), profile
        )
        expected_cycles = 6.0 / 1.84
        self.assertAlmostEqual(
            report["predicted"]["cycles_per_group"], expected_cycles
        )
        self.assertEqual(
            report["predicted"]["bottlenecks"], ["compute_store"]
        )
        self.assertAlmostEqual(
            report["result"]["compute_ipc"], 4.0 / expected_cycles
        )
        self.assertEqual(report["traffic"]["vector_bytes"], 32.0)
        self.assertEqual(report["traffic"]["ops_per_compute"], 16.0)

    def test_v1_measured_matrix_matches_documented_error_budget(self):
        profile = issue_model.PROFILES["neoverse-v1-sve256-fmla"]
        measured_cycles = {
            "4F": 2.175283,
            "3L": 1.509333,
            "1F+2L": 1.005306,
            "1F+3L": 1.512034,
            "2F+3L": 1.638261,
            "3F+3L": 1.693180,
            "4F+1L": 2.178675,
            "4F+2L": 2.178612,
            "4F+3L": 2.202526,
            "4F+4L": 2.221060,
            "4F+5L": 2.724299,
            "1S": 1.054046,
            "2S": 2.002887,
            "3L+1S": 2.180414,
            "3L+2S": 2.503619,
            "4F+1S": 2.717595,
            "4F+3L+1S": 2.722874,
            "4F+3L+2S": 3.263184,
        }
        within_two_percent = 0
        for mix_text, observed_cycles in measured_cycles.items():
            with self.subTest(mix=mix_text):
                report = issue_model.evaluate(
                    issue_model.parse_mix(mix_text),
                    profile,
                    observed_cycles_per_group=observed_cycles,
                )
                attainment = report["efficiency"]["model_attainment"]
                self.assertGreaterEqual(attainment, 0.91)
                self.assertLessEqual(attainment, 1.01)
                if 0.98 <= attainment <= 1.02:
                    within_two_percent += 1
        self.assertGreaterEqual(within_two_percent, 14)

    def test_v1_bfmmla_profile_uses_sve256_operation_count(self):
        profile = issue_model.PROFILES["neoverse-v1-sve256-bfmmla"]

        pure = issue_model.evaluate(issue_model.parse_mix("4F"), profile)
        self.assertAlmostEqual(
            pure["predicted"]["cycles_per_group"], 2.0
        )
        self.assertEqual(
            pure["constraint_utilization"]["compute_when_loading"], 0.0
        )

        mixed = issue_model.evaluate(
            issue_model.parse_mix("4F+3L+2S"), profile
        )
        expected_cycles = 6.0 / 1.86
        self.assertAlmostEqual(
            mixed["predicted"]["cycles_per_group"], expected_cycles
        )
        self.assertEqual(
            mixed["predicted"]["bottlenecks"],
            ["compute_store_when_loading"],
        )
        self.assertEqual(mixed["traffic"]["vector_bytes"], 32.0)
        self.assertEqual(mixed["traffic"]["ops_per_compute"], 64.0)

    def test_v1_bfmmla_matrix_matches_documented_error_budget(self):
        profile = issue_model.PROFILES["neoverse-v1-sve256-bfmmla"]
        measured_cycles = {
            "4F": 2.003204,
            "2L": 1.003002,
            "1F+1L": 0.546337,
            "1F+2L": 1.003186,
            "2F+1L": 1.089155,
            "2F+2L": 1.089918,
            "3F+3L": 1.634157,
            "4F+4L": 2.178412,
            "3F+4L": 2.174401,
            "1S": 1.002947,
            "2S": 2.003248,
            "1F+1S": 1.004096,
            "4F+1S": 2.503820,
            "1F+1L+1S": 1.056453,
            "2F+2L+1S": 1.628748,
            "3F+3L+1S": 2.194297,
            "4F+4L+1S": 2.722037,
            "4F+3L+1S": 2.719262,
            "4F+3L+2S": 3.226180,
        }
        within_two_percent = 0
        for mix_text, observed_cycles in measured_cycles.items():
            with self.subTest(mix=mix_text):
                report = issue_model.evaluate(
                    issue_model.parse_mix(mix_text),
                    profile,
                    observed_cycles_per_group=observed_cycles,
                )
                attainment = report["efficiency"]["model_attainment"]
                self.assertGreaterEqual(attainment, 0.91)
                self.assertLessEqual(attainment, 1.03)
                if 0.98 <= attainment <= 1.02:
                    within_two_percent += 1
        self.assertGreaterEqual(within_two_percent, 18)

    def test_cli_json_output(self):
        completed = subprocess.run(
            [sys.executable, str(MODEL_PATH), "1F+2L", "--json"],
            check=True,
            capture_output=True,
            text=True,
        )
        report = json.loads(completed.stdout)
        self.assertAlmostEqual(report["result"]["compute_ipc"], 1.5)
        self.assertAlmostEqual(report["result"]["load_ipc"], 3.0)

    def test_cli_selects_v1_profile(self):
        completed = subprocess.run(
            [
                sys.executable,
                str(MODEL_PATH),
                "4F+3L+2S",
                "--profile",
                "neoverse-v1-sve256-fmla",
                "--json",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        report = json.loads(completed.stdout)
        self.assertEqual(report["profile"], "neoverse-v1-sve256-fmla")
        self.assertEqual(report["traffic"]["vector_bytes"], 32.0)
        self.assertEqual(report["traffic"]["ops_per_compute"], 16.0)

    def test_cli_selects_v1_bfmmla_profile(self):
        completed = subprocess.run(
            [
                sys.executable,
                str(MODEL_PATH),
                "2F+2L",
                "--profile",
                "neoverse-v1-sve256-bfmmla",
                "--json",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        report = json.loads(completed.stdout)
        self.assertEqual(report["profile"], "neoverse-v1-sve256-bfmmla")
        self.assertEqual(report["traffic"]["vector_bytes"], 32.0)
        self.assertEqual(report["traffic"]["ops_per_compute"], 64.0)


if __name__ == "__main__":
    unittest.main()
