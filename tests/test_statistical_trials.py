#!/usr/bin/env python3

import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "run_statistical_trials.py"
SPEC = importlib.util.spec_from_file_location("run_statistical_trials", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class StatisticalTrialsTest(unittest.TestCase):
    def test_ten_sample_summary(self):
        values = list(range(1, 11))
        median, q1, q3, iqr, low, high = MODULE.summarize(values)
        self.assertEqual(median, 5.5)
        self.assertEqual(q1, 3.25)
        self.assertEqual(q3, 7.75)
        self.assertEqual(iqr, 4.5)
        self.assertAlmostEqual(low, 3.334300, places=5)
        self.assertAlmostEqual(high, 7.665700, places=5)

    def test_aggregate_cpufb_sections(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for trial in range(1, 3):
                path = Path(directory) / f"trial-{trial}.csv"
                with path.open("w", newline="") as handle:
                    writer = csv.writer(handle)
                    writer.writerow(["section", "Instruction Set", "Core Computation", "Peak Performance", "IPC", "Latency"])
                    writer.writerow(["compute", "asimd", "fmla", f"{trial * 10} GFLOPS", str(trial), f"{trial + 2} cycles"])
                paths.append(path)
            samples, units = MODULE.aggregate(paths)
            key = ("compute", "asimd / fmla", "Peak Performance")
            self.assertEqual(samples[key], [10.0, 20.0])
            self.assertEqual(units[key], "GFLOPS")

    def test_canonicalizes_tera_units(self):
        self.assertEqual(MODULE.canonicalize_unit(1.25, "TFLOPS"),
                         (1250.0, "GFLOPS"))

    def test_requires_complete_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trial.csv"
            with path.open("w", newline="") as handle:
                writer = csv.writer(handle)
                writer.writerow(["section", "Item", "Value", "Source"])
                writer.writerow(["system", "Sample Timestamp", "now", "clock"])
                writer.writerow(["system", "Core Selection", "[0]", "cli"])
                writer.writerow(["system", "Temperature", "unavailable", "OS"])
                writer.writerow(["system", "CPU Frequency", "unavailable", "OS"])
                writer.writerow(["section", "Core ID", "Counter Source"])
                writer.writerow(["freq", "0", "perf_event cycles"])
            with self.assertRaisesRegex(ValueError, "Core Migration"):
                MODULE.validate_metadata([path])

    def test_requires_exactly_ten_distinct_trials(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for trial in range(10):
                path = Path(directory) / f"trial-{trial}.csv"
                path.touch()
                paths.append(path)
            MODULE.require_trial_count(paths)
            with self.assertRaisesRegex(ValueError, "exactly 10 distinct"):
                MODULE.require_trial_count(paths[:9])
            alias = Path(directory) / "alias.csv"
            alias.symlink_to(paths[0])
            with self.assertRaisesRegex(ValueError, "exactly 10 distinct"):
                MODULE.require_trial_count(paths[:9] + [alias])
            hard_link = Path(directory) / "hard-link.csv"
            hard_link.hardlink_to(paths[0])
            with self.assertRaisesRegex(ValueError, "exactly 10 distinct"):
                MODULE.require_trial_count(paths[:9] + [hard_link])
            with self.assertRaisesRegex(ValueError, "does not exist"):
                MODULE.require_trial_count(paths[:9] + [Path(directory) / "missing.csv"])

    def test_rejects_metric_missing_from_one_trial(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for trial in range(2):
                path = Path(directory) / f"trial-{trial}.csv"
                with path.open("w", newline="") as handle:
                    writer = csv.writer(handle)
                    writer.writerow(["section", "Instruction Set", "Core Computation", "Peak Performance", "IPC"])
                    writer.writerow(["compute", "asimd", "fmla", "10 GFLOPS", "" if trial else "2"])
                paths.append(path)
            with self.assertRaisesRegex(ValueError, "inconsistent metric set"):
                MODULE.aggregate(paths)

    def test_rejects_duplicate_metric_in_trial(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trial.csv"
            with path.open("w", newline="") as handle:
                writer = csv.writer(handle)
                writer.writerow(["section", "Instruction Set", "Core Computation", "IPC"])
                writer.writerow(["compute", "asimd", "fmla", "2"])
                writer.writerow(["compute", "asimd", "fmla", "2"])
            with self.assertRaisesRegex(ValueError, "duplicate metric"):
                MODULE.aggregate([path])

    def test_aggregates_standalone_memory_mode(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for trial in range(2):
                path = Path(directory) / f"trial-{trial}.csv"
                with path.open("w", newline="") as handle:
                    writer = csv.writer(handle)
                    writer.writerow(["section", "Core ID", "Workset", "Kernel", "Median GB/s", "Load IPC"])
                    writer.writerow(["memory_bandwidth", "0", "4 MiB", "ldp", str(20 + trial), "2"])
                paths.append(path)
            samples, units = MODULE.aggregate(paths)
            self.assertEqual(
                samples[("memory_bandwidth", "0 / 4 MiB / ldp", "Median GB/s")],
                [20.0, 21.0],
            )

    def test_rejects_outputless_success_instead_of_reusing_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory)
            stale = output_dir / "trial-01.csv"
            stale.write_text("stale", encoding="utf-8")
            args = type("Args", (), {
                "output_dir": directory,
                "trials": 1,
                "command": ["/usr/bin/true"],
            })()
            self.assertEqual(MODULE.run_trials(args), 1)
            self.assertFalse(stale.exists())


if __name__ == "__main__":
    unittest.main()