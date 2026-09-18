#!/usr/bin/env python3

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = ROOT / "tools" / "pair_issue_test.py"
SPEC = importlib.util.spec_from_file_location("pair_issue_test", TOOL_PATH)
pair_issue = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = pair_issue
SPEC.loader.exec_module(pair_issue)


class PairIssueTest(unittest.TestCase):
    def test_normalized_counts_use_measured_peaks(self):
        count_a, count_b = pair_issue.normalized_counts(4.0, 6.0, 0.5)
        self.assertEqual(count_a / 4.0, count_b / 6.0)
        self.assertLessEqual(count_a + count_b, 64)

        low_a, high_b = pair_issue.normalized_counts(4.0, 6.0, 0.25)
        self.assertAlmostEqual(
            (low_a / 4.0) / (high_b / 6.0), 1.0 / 3.0
        )

    def test_interleaver_preserves_counts_and_spreads_a(self):
        tokens = pair_issue.schedule_tokens(3, 7, "interleaved")
        self.assertEqual(tokens.count("A"), 3)
        self.assertEqual(tokens.count("B"), 7)
        for prefix_length in range(1, len(tokens) + 1):
            actual = tokens[:prefix_length].count("A")
            ideal = prefix_length * 3.0 / 10.0
            self.assertLessEqual(abs(actual - ideal), 1.0)

    def test_shifted_schedule_changes_phase_not_ratio(self):
        normal = pair_issue.schedule_tokens(3, 7, "interleaved")
        shifted = pair_issue.schedule_tokens(3, 7, "shifted")
        self.assertNotEqual(normal, shifted)
        self.assertEqual(normal.count("A"), shifted.count("A"))
        self.assertEqual(normal.count("B"), shifted.count("B"))

    def test_body_expansion_keeps_whole_patterns(self):
        layout = pair_issue.make_body_layout(2, 3, "blocked", 13)
        self.assertEqual(layout.pattern_repetitions, 3)
        self.assertEqual(layout.body_a, 6)
        self.assertEqual(layout.body_b, 9)
        self.assertEqual(len(layout.tokens), 15)

    def test_body_closes_both_register_rotation_periods(self):
        layout = pair_issue.make_body_layout(
            44, 12, "interleaved", 256, 16, 16
        )
        self.assertEqual(layout.pattern_repetitions, 8)
        self.assertEqual(layout.body_a % 16, 0)
        self.assertEqual(layout.body_b % 16, 0)

        coprime = pair_issue.make_body_layout(
            25, 27, "interleaved", 256, 16, 16
        )
        self.assertEqual(coprime.pattern_repetitions, 16)
        self.assertEqual(coprime.body_a % 16, 0)
        self.assertEqual(coprime.body_b % 16, 0)

    def test_overlap_metric_has_serial_and_ideal_endpoints(self):
        base = dict(
            alpha=0.5,
            schedule="interleaved",
            base_a=4,
            base_b=2,
            body_a=4,
            body_b=2,
            pattern_repetitions=1,
            retired_per_loop=8.0,
            repeat_cycles=[1.0],
            repeat_retired=[8.0],
        )
        ideal = pair_issue.Measurement(cycles_per_loop=1.0, **base)
        metrics = pair_issue.measurement_metrics(ideal, 4.0, 2.0)
        self.assertAlmostEqual(metrics["ipc_a"], 4.0)
        self.assertAlmostEqual(metrics["ipc_b"], 2.0)
        self.assertAlmostEqual(metrics["overlap"], 1.0)

        serial = pair_issue.Measurement(cycles_per_loop=2.0, **base)
        metrics = pair_issue.measurement_metrics(serial, 4.0, 2.0)
        self.assertAlmostEqual(metrics["overlap"], 0.0)

    def test_knee_refinement_bisects_only_crossing_interval(self):
        points = [
            pair_issue.PointSummary(0.0, 0.0, 1.0, None),
            pair_issue.PointSummary(0.50, 0.80, 1.0, 0.8),
            pair_issue.PointSummary(0.75, 0.99, 0.8, 0.8),
            pair_issue.PointSummary(1.0, 1.0, 0.0, None),
        ]
        candidates = pair_issue.refinement_candidates(
            points, tolerance=0.03, protect_threshold=0.97, protect="a"
        )
        self.assertEqual(candidates[0][1], 0.625)
        self.assertIn("A-knee", candidates[0][2])

    def test_nonlinear_overlap_requests_local_refinement(self):
        points = [
            pair_issue.PointSummary(0.25, 0.7, 1.0, 0.4),
            pair_issue.PointSummary(0.50, 1.0, 1.0, 0.9),
            pair_issue.PointSummary(0.75, 1.0, 0.7, 0.4),
        ]
        candidates = pair_issue.refinement_candidates(
            points, tolerance=0.03, protect_threshold=0.97, protect="none"
        )
        self.assertEqual({item[1] for item in candidates}, {0.375, 0.625})
        self.assertTrue(
            all(item[2] == "nonlinear-overlap" for item in candidates)
        )

    def test_sve_neon_assembly_uses_disjoint_vector_numbers(self):
        sve = pair_issue.INSTRUCTION_SPECS["sve-fmla"]
        neon = pair_issue.INSTRUCTION_SPECS["neon-fmla"]
        registers = pair_issue.assign_registers(sve, neon)
        self.assertTrue(set(registers.a).isdisjoint(registers.b))
        layout = pair_issue.make_body_layout(2, 2, "interleaved", 4)
        assembly = pair_issue.generate_assembly(
            sve, neon, registers, layout
        )
        self.assertIn("fmla z0.s", assembly)
        self.assertIn("fmla z1.s", assembly)
        self.assertIn("fmla v16.4s", assembly)
        self.assertIn("fmla v17.4s", assembly)
        self.assertNotIn("fmla v0.4s", assembly)

    def test_sve_ld1h_is_the_only_load_class(self):
        load_specs = [
            spec
            for spec in pair_issue.INSTRUCTION_SPECS.values()
            if spec.requires_memory
        ]
        self.assertEqual([spec.name for spec in load_specs], ["sve-ld1h"])

    def test_sve_fmla_ld1h_assembly_uses_disjoint_registers(self):
        fmla = pair_issue.INSTRUCTION_SPECS["sve-fmla"]
        load = pair_issue.INSTRUCTION_SPECS["sve-ld1h"]
        registers = pair_issue.assign_registers(fmla, load)
        self.assertTrue(set(registers.a).isdisjoint(registers.b))
        layout = pair_issue.make_body_layout(2, 2, "interleaved", 4)
        assembly = pair_issue.generate_assembly(
            fmla, load, registers, layout
        )
        self.assertIn("ptrue p0.s", assembly)
        self.assertIn("ptrue p1.h", assembly)
        self.assertIn("fmla z0.s", assembly)
        self.assertIn("fmla z1.s", assembly)
        self.assertIn("ld1h z16.h, p1/z, [x1]", assembly)
        self.assertIn("ld1h z17.h, p1/z, [x1, #1, MUL VL]", assembly)
        self.assertNotIn("dup z16", assembly)

    def test_driver_passes_an_l1_sized_load_buffer(self):
        driver = pair_issue.generate_driver()
        self.assertIn("LOAD_BUFFER_BYTES = 4096", driver)
        self.assertIn("__attribute__((aligned(4096)))", driver)
        self.assertIn("cpufb_pair_probe(loops, load_buffer)", driver)

    def test_scalar_register_pool_has_a_saved_sixteenth_chain(self):
        scalar = pair_issue.INSTRUCTION_SPECS["scalar-add"]
        load = pair_issue.INSTRUCTION_SPECS["sve-ld1h"]
        registers = pair_issue.assign_registers(scalar, load)
        self.assertEqual(len(registers.a), 16)
        self.assertEqual(registers.a[-1], 19)
        layout = pair_issue.make_body_layout(
            16, 16, "interleaved", 32, 16, 16
        )
        assembly = pair_issue.generate_assembly(
            scalar, load, registers, layout
        )
        self.assertIn("stp x19, x20", assembly)
        self.assertIn("add x19, x19, #1", assembly)
        self.assertIn("ldp x19, x20", assembly)

    def test_perf_csv_parser(self):
        sample = """123456,,cycles:u,100.00,,\n654321,,instructions:u,100.00,,\n"""
        counters = pair_issue.parse_perf_stat(sample)
        self.assertEqual(counters.cycles, 123456)
        self.assertEqual(counters.instructions, 654321)

    def test_cli_dry_run_json(self):
        completed = subprocess.run(
            [
                sys.executable,
                str(TOOL_PATH),
                "sve-fmla",
                "sve-ld1h",
                "--dry-run",
                "--peak-a",
                "4",
                "--peak-b",
                "6",
                "--format",
                "json",
            ],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        report = json.loads(completed.stdout)
        self.assertEqual(report["feature"], "experimental-pair-issue-dry-run")
        self.assertEqual(len(report["plans"]), 3)
        middle = report["plans"][1]
        self.assertEqual(
            middle["normalized_pressure_a"],
            middle["normalized_pressure_b"],
        )


if __name__ == "__main__":
    unittest.main()
