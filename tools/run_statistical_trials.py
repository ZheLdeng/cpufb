#!/usr/bin/env python3
"""Run independent cpufb trials and aggregate numeric table cells."""

import argparse
import csv
import math
import re
import statistics
import subprocess
import sys
from pathlib import Path


T_CRITICAL_975 = {
    1: 12.706,
    2: 4.303,
    3: 3.182,
    4: 2.776,
    5: 2.571,
    6: 2.447,
    7: 2.365,
    8: 2.306,
    9: 2.262,
    10: 2.228,
    11: 2.201,
    12: 2.179,
    13: 2.160,
    14: 2.145,
    15: 2.131,
    16: 2.120,
    17: 2.110,
    18: 2.101,
    19: 2.093,
    20: 2.086,
    21: 2.080,
    22: 2.074,
    23: 2.069,
    24: 2.064,
    25: 2.060,
    26: 2.056,
    27: 2.052,
    28: 2.048,
    29: 2.045,
}

NUMBER = re.compile(r"(?<![A-Za-z0-9_.])[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?")
IDENTIFIER_COLUMNS = {
    "system": {"Item"},
    "compute": {"Instruction Set", "Core Computation"},
    "load": {"Cache Level", "Core Instruction"},
    "cache": {"Item"},
    "freq": {"Core ID"},
    "multi_issue": {"Item", "Core Instruction"},
    "memory_bandwidth": {"Core ID", "Workset", "Kernel"},
}
METRIC_COLUMNS = {
    "compute": {"Peak Performance", "IPC", "Latency"},
    "load": {"Bandwidth (per core)", "Cache Capacity", "Workset", "Bandwidth (GB/s)"},
    "cache": {"Topology / Core", "Probe / Kernel", "Median Bandwidth", "Workset"},
    "freq": {"Theory Freq", "Test Freq", "IPC(FSU32)", "IPC(FSU64)",
             "IPC(LSU ldr)", "IPC(SVE32)", "IPC(SVE64)"},
    "multi_issue": {"IPC"},
    "memory_bandwidth": {"Median GB/s", "B/cycle", "Load IPC", "Min GB/s",
                         "Max GB/s"},
}
REQUIRED_SYSTEM_ITEMS = {
    "Sample Timestamp", "Core Selection", "Temperature", "CPU Frequency",
    "Core Migration",
}
TRIAL_COUNT = 10


def percentile(sorted_values, probability):
    if len(sorted_values) == 1:
        return sorted_values[0]
    position = (len(sorted_values) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return sorted_values[lower]
    weight = position - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def summarize(values):
    ordered = sorted(values)
    count = len(ordered)
    median = statistics.median(ordered)
    q1 = percentile(ordered, 0.25)
    q3 = percentile(ordered, 0.75)
    mean = statistics.mean(ordered)
    if count > 1:
        critical = T_CRITICAL_975.get(count - 1, 1.96)
        half_width = critical * statistics.stdev(ordered) / math.sqrt(count)
    else:
        half_width = 0.0
    return median, q1, q3, q3 - q1, mean - half_width, mean + half_width


def parse_number(cell):
    matches = list(NUMBER.finditer(cell))
    if len(matches) != 1:
        return None
    match = matches[0]
    value = float(match.group(0))
    unit = (cell[:match.start()] + cell[match.end():]).strip()
    return value, unit


def canonicalize_unit(value, unit):
    conversions = {
        "TFLOPS": (1000.0, "GFLOPS"),
        "TOPS": (1000.0, "GOPS"),
        "MiB": (1024.0, "KiB"),
        "MB": (1024.0, "KB"),
    }
    scale, canonical = conversions.get(unit, (1.0, unit))
    return value * scale, canonical


def read_sections(path):
    sections = {}
    headers = {}
    pending_header = None
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.reader(handle):
            if not row:
                continue
            if row[0] == "section":
                pending_header = row[1:]
                continue
            current = row[0]
            if pending_header is not None:
                headers[current] = pending_header
                sections.setdefault(current, [])
                pending_header = None
            if current not in headers:
                raise ValueError(f"{path}: data row precedes section header: {current}")
            sections[current].append(dict(zip(headers[current], row[1:])))
    return sections, headers


def row_key(section, row, headers):
    identifiers = IDENTIFIER_COLUMNS.get(section, set())
    values = [row[name] for name in headers if name in identifiers and row.get(name)]
    return " / ".join(values) if values else "summary"


def validate_metadata(paths):
    for path in paths:
        sections, headers = read_sections(path)
        if not {"Item", "Value", "Source"}.issubset(headers.get("system", [])):
            raise ValueError(f"{path}: incomplete system metadata header")
        system_rows = sections.get("system", [])
        system_items = {row.get("Item") for row in system_rows}
        missing = REQUIRED_SYSTEM_ITEMS - system_items
        if missing:
            raise ValueError(
                f"{path}: missing system metadata: {', '.join(sorted(missing))}"
            )
        for item in REQUIRED_SYSTEM_ITEMS:
            rows = [row for row in system_rows if row.get("Item") == item]
            if len(rows) != 1 or not rows[0].get("Value") or not rows[0].get("Source"):
                raise ValueError(f"{path}: incomplete system metadata: {item}")

        has_counter_source = False
        for section, rows in sections.items():
            source_columns = {
                column for column in headers.get(section, [])
                if column in {"Counter Source", "Cycle Source"}
            }
            if any(row.get(column) for row in rows for column in source_columns):
                has_counter_source = True
                break
        if not has_counter_source:
            raise ValueError(f"{path}: missing counter or cycle source metadata")


def aggregate(paths):
    samples = {}
    units = {}
    expected_keys = None
    for path in paths:
        trial_samples = {}
        sections, headers = read_sections(path)
        for section, rows in sections.items():
            identifiers = IDENTIFIER_COLUMNS.get(section, set())
            metrics = METRIC_COLUMNS.get(section, set())
            load_level = ""
            for row in rows:
                if section == "load":
                    if row.get("Cache Level") not in ("", "--------"):
                        load_level = row["Cache Level"]
                    elif load_level:
                        row = dict(row)
                        row["Cache Level"] = load_level
                key = row_key(section, row, headers[section])
                for metric in headers[section]:
                    if metric in identifiers or metric not in metrics:
                        continue
                    parsed = parse_number(row.get(metric, ""))
                    if parsed is None:
                        continue
                    value, unit = parsed
                    value, unit = canonicalize_unit(value, unit)
                    sample_key = (section, key, metric)
                    if sample_key in units and units[sample_key] != unit:
                        raise ValueError(
                            f"inconsistent unit for {sample_key}: {units[sample_key]!r} vs {unit!r}"
                        )
                    if sample_key in trial_samples:
                        raise ValueError(f"{path}: duplicate metric: {sample_key}")
                    units[sample_key] = unit
                    trial_samples[sample_key] = value
        trial_keys = set(trial_samples)
        if not trial_keys:
            raise ValueError(f"{path}: no supported numeric metrics")
        if expected_keys is None:
            expected_keys = trial_keys
        elif trial_keys != expected_keys:
            missing = expected_keys - trial_keys
            unexpected = trial_keys - expected_keys
            details = []
            if missing:
                details.append(f"missing {len(missing)} metric(s), including {min(missing)!r}")
            if unexpected:
                details.append(
                    f"unexpected {len(unexpected)} metric(s), including {min(unexpected)!r}"
                )
            raise ValueError(f"{path}: inconsistent metric set: {'; '.join(details)}")
        for sample_key, value in trial_samples.items():
            samples.setdefault(sample_key, []).append(value)
    return samples, units


def require_trial_count(paths):
    try:
        file_ids = {
            (path.resolve(strict=True).stat().st_dev,
             path.resolve(strict=True).stat().st_ino)
            for path in paths
        }
    except FileNotFoundError as error:
        raise ValueError(f"trial CSV file does not exist: {error.filename}") from error
    if len(paths) != TRIAL_COUNT or len(file_ids) != TRIAL_COUNT:
        raise ValueError(f"exactly {TRIAL_COUNT} distinct trial CSV files are required")


def write_summary(paths, output):
    samples, units = aggregate(paths)
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "section", "row", "metric", "unit", "n", "median", "q1",
            "q3", "iqr", "mean_ci95_low", "mean_ci95_high", "samples",
        ])
        for key in sorted(samples):
            values = samples[key]
            median, q1, q3, iqr, low, high = summarize(values)
            writer.writerow([
                *key, units[key], len(values), f"{median:.9g}", f"{q1:.9g}",
                f"{q3:.9g}", f"{iqr:.9g}", f"{low:.9g}", f"{high:.9g}",
                ";".join(f"{value:.9g}" for value in values),
            ])


def run_trials(args):
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_paths = []
    for trial in range(1, args.trials + 1):
        csv_path = output_dir / f"trial-{trial:02d}.csv"
        log_path = output_dir / f"trial-{trial:02d}.log"
        csv_path.unlink(missing_ok=True)
        log_path.unlink(missing_ok=True)
        command = list(args.command) + [f"--save={csv_path}", "--save-format=csv"]
        with log_path.open("w", encoding="utf-8") as log:
            completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        if completed.returncode != 0:
            print(f"trial {trial} failed; see {log_path}", file=sys.stderr)
            return completed.returncode
        if not csv_path.is_file():
            print(f"trial {trial} produced no CSV; see {log_path}", file=sys.stderr)
            return 1
        try:
            validate_metadata([csv_path])
        except ValueError as error:
            print(error, file=sys.stderr)
            return 1
        csv_paths.append(csv_path)
        print(f"completed trial {trial}/{args.trials}", flush=True)
    write_summary(csv_paths, output_dir / "summary.csv")
    return 0


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="action", required=True)
    run_parser = subparsers.add_parser("run", help="run cpufb repeatedly and aggregate")
    run_parser.add_argument("--trials", type=int, default=TRIAL_COUNT)
    run_parser.add_argument("--output-dir", required=True)
    run_parser.add_argument("command", nargs=argparse.REMAINDER)
    aggregate_parser = subparsers.add_parser("aggregate", help="aggregate existing CSV files")
    aggregate_parser.add_argument("--output", required=True)
    aggregate_parser.add_argument("inputs", nargs="+")
    args = parser.parse_args()

    if args.action == "run":
        if args.command and args.command[0] == "--":
            args.command = args.command[1:]
        if args.trials != TRIAL_COUNT or not args.command:
            parser.error(f"run requires --trials={TRIAL_COUNT} and a command after --")
        return run_trials(args)

    paths = [Path(path) for path in args.inputs]
    try:
        require_trial_count(paths)
    except ValueError as error:
        parser.error(str(error))
    validate_metadata(paths)
    write_summary(paths, Path(args.output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())