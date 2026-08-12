#!/usr/bin/env python3
"""Experimental sparse/adaptive pair-issue probe for Linux AArch64.

The probe measures two instruction classes in isolation, creates normalized
mixed ratios from those measured peaks, and only adds ratios when the measured
frontier is nonlinear or a protected pure peak crosses the requested threshold.

Generated assembly is deliberately dependency-free across accumulator chains.
When both classes use the architectural SIMD/SVE register bank, their register
numbers are disjoint (Zn and Vn alias on AArch64).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


@dataclass(frozen=True)
class InstructionSpec:
    name: str
    description: str
    register_bank: str
    requires_sve: bool = False
    requires_memory: bool = False

    def initialize(self, register: int) -> Optional[str]:
        if self.name == "sve-ld1h":
            return None
        if self.name == "sve-fmla":
            return f"dup z{register}.s, #0"
        if self.name in ("neon-fmla", "scalar-fadd"):
            return f"movi v{register}.4s, #0"
        if self.name == "scalar-add":
            return f"mov x{register}, #0"
        raise ValueError(f"no initializer for {self.name}")

    def instruction(self, register: int) -> str:
        if self.name == "sve-ld1h":
            offset = register % 8
            address = (
                "[x1]" if offset == 0 else f"[x1, #{offset}, MUL VL]"
            )
            return f"ld1h z{register}.h, p1/z, {address}"
        if self.name == "sve-fmla":
            return (
                f"fmla z{register}.s, p0/m, "
                f"z{register}.s, z{register}.s"
            )
        if self.name == "neon-fmla":
            return (
                f"fmla v{register}.4s, v{register}.4s, "
                f"v{register}.4s"
            )
        if self.name == "scalar-fadd":
            return f"fadd s{register}, s{register}, s{register}"
        if self.name == "scalar-add":
            return f"add x{register}, x{register}, #1"
        raise ValueError(f"no instruction emitter for {self.name}")


INSTRUCTION_SPECS: Dict[str, InstructionSpec] = {
    "sve-ld1h": InstructionSpec(
        "sve-ld1h",
        "SVE contiguous halfword load (default load class)",
        "vector",
        True,
        True,
    ),
    "sve-fmla": InstructionSpec(
        "sve-fmla", "SVE predicated FP32 FMLA", "vector", True
    ),
    "neon-fmla": InstructionSpec(
        "neon-fmla", "NEON FP32 vector FMLA", "vector"
    ),
    "scalar-add": InstructionSpec(
        "scalar-add", "integer scalar ADD-immediate", "gpr"
    ),
    "scalar-fadd": InstructionSpec(
        "scalar-fadd", "scalar FP32 FADD", "vector"
    ),
}

SCHEDULES = ("interleaved", "blocked", "shifted")


@dataclass(frozen=True)
class RegisterAssignment:
    a: Tuple[int, ...]
    b: Tuple[int, ...]


@dataclass(frozen=True)
class BodyLayout:
    base_a: int
    base_b: int
    body_a: int
    body_b: int
    pattern_repetitions: int
    schedule: str
    tokens: Tuple[str, ...]


@dataclass(frozen=True)
class CounterSample:
    cycles: float
    instructions: float


@dataclass
class Measurement:
    alpha: float
    schedule: str
    base_a: int
    base_b: int
    body_a: int
    body_b: int
    pattern_repetitions: int
    cycles_per_loop: float
    retired_per_loop: float
    repeat_cycles: List[float]
    repeat_retired: List[float]


@dataclass(frozen=True)
class PointSummary:
    alpha: float
    efficiency_a: float
    efficiency_b: float
    overlap: Optional[float]


def assign_registers(
    spec_a: InstructionSpec, spec_b: InstructionSpec
) -> RegisterAssignment:
    vector_roles = sum(
        spec.register_bank == "vector" for spec in (spec_a, spec_b)
    )
    if vector_roles == 2:
        # Zn and Vn are aliases. Splitting by architectural register number
        # avoids hidden dependencies for SVE+NEON and SIMD+scalar-FP tests.
        vector_pools = [tuple(range(0, 16)), tuple(range(16, 32))]
    else:
        vector_pools = [tuple(range(0, 16))]

    vector_index = 0
    assigned: List[Tuple[int, ...]] = []
    for spec in (spec_a, spec_b):
        if spec.register_bank == "vector":
            assigned.append(vector_pools[vector_index])
            vector_index += 1
        else:
            # x0/x1 are arguments, x2 is the loop counter, and x18 is reserved
            # on several AArch64 platforms. Add saved x19 to the caller-saved
            # x3-x17 pool so every instruction class has a 16-register period.
            assigned.append(tuple(range(3, 18)) + (19,))
    return RegisterAssignment(assigned[0], assigned[1])


def normalized_counts(
    peak_a: float,
    peak_b: float,
    alpha: float,
    max_period: int = 64,
) -> Tuple[int, int]:
    """Approximate a:b = alpha*Pa : (1-alpha)*Pb sparsely.

    All integer periods up to ``max_period`` are considered. Exact ratios use
    the longest fitting multiple, which gives the interleaver more scheduling
    positions without changing the requested pressure ratio.
    """
    if peak_a <= 0.0 or peak_b <= 0.0:
        raise ValueError("pure IPC peaks must be positive")
    if not 0.0 < alpha < 1.0:
        raise ValueError("alpha must be strictly between zero and one")
    if max_period < 2:
        raise ValueError("max_period must be at least two")

    weight_a = alpha * peak_a
    weight_b = (1.0 - alpha) * peak_b
    target_share = weight_a / (weight_a + weight_b)
    best: Optional[Tuple[float, int, int, int]] = None
    for total in range(2, max_period + 1):
        count_a = max(1, min(total - 1, round(total * target_share)))
        count_b = total - count_a
        observed_share = count_a / total
        error = abs(observed_share - target_share)
        candidate = (error, -total, count_a, count_b)
        if best is None or candidate < best:
            best = candidate
    assert best is not None
    return best[2], best[3]


def schedule_tokens(count_a: int, count_b: int, schedule: str) -> List[str]:
    if count_a < 0 or count_b < 0 or count_a + count_b == 0:
        raise ValueError("instruction counts must be non-negative and nonzero")
    if schedule not in SCHEDULES:
        raise ValueError(f"unknown schedule: {schedule}")
    if count_a == 0:
        return ["B"] * count_b
    if count_b == 0:
        return ["A"] * count_a
    if schedule == "blocked":
        return ["A"] * count_a + ["B"] * count_b

    total = count_a + count_b
    accumulator = 0 if schedule == "interleaved" else total // 2
    tokens: List[str] = []
    for _ in range(total):
        accumulator += count_a
        if accumulator >= total:
            accumulator -= total
            tokens.append("A")
        else:
            tokens.append("B")
    assert tokens.count("A") == count_a
    assert tokens.count("B") == count_b
    return tokens


def make_body_layout(
    count_a: int,
    count_b: int,
    schedule: str,
    target_body_instructions: int,
    register_period_a: int = 1,
    register_period_b: int = 1,
) -> BodyLayout:
    base = schedule_tokens(count_a, count_b, schedule)
    minimum_repetitions = max(
        1, math.ceil(target_body_instructions / len(base))
    )

    # The generated loop restarts at the first static instruction. Make the
    # per-loop A/B counts whole register-rotation periods; otherwise a loop
    # boundary can reuse z0/x3 much sooner than the intended 16-chain distance.
    period_a = (
        register_period_a // math.gcd(count_a, register_period_a)
        if count_a
        else 1
    )
    period_b = (
        register_period_b // math.gcd(count_b, register_period_b)
        if count_b
        else 1
    )
    repetition_period = math.lcm(period_a, period_b)
    repetitions = (
        math.ceil(minimum_repetitions / repetition_period)
        * repetition_period
    )
    tokens = tuple(base * repetitions)
    return BodyLayout(
        base_a=count_a,
        base_b=count_b,
        body_a=tokens.count("A"),
        body_b=tokens.count("B"),
        pattern_repetitions=repetitions,
        schedule=schedule,
        tokens=tokens,
    )


def _preserve_vector_registers() -> List[str]:
    return [
        "stp d8, d9, [sp, #-16]!",
        "stp d10, d11, [sp, #-16]!",
        "stp d12, d13, [sp, #-16]!",
        "stp d14, d15, [sp, #-16]!",
    ]


def _restore_vector_registers() -> List[str]:
    return [
        "ldp d14, d15, [sp], #16",
        "ldp d12, d13, [sp], #16",
        "ldp d10, d11, [sp], #16",
        "ldp d8, d9, [sp], #16",
    ]


def _uses_register_bank(
    spec_a: InstructionSpec, spec_b: InstructionSpec, bank: str
) -> bool:
    return spec_a.register_bank == bank or spec_b.register_bank == bank


def generate_assembly(
    spec_a: InstructionSpec,
    spec_b: InstructionSpec,
    registers: RegisterAssignment,
    layout: BodyLayout,
) -> str:
    lines = [
        ".text",
        ".p2align 6",
        ".global cpufb_pair_probe",
        ".type cpufb_pair_probe, %function",
        "cpufb_pair_probe:",
    ]
    if _uses_register_bank(spec_a, spec_b, "gpr"):
        lines.append("    stp x19, x20, [sp, #-16]!")
    if _uses_register_bank(spec_a, spec_b, "vector"):
        lines.extend(f"    {line}" for line in _preserve_vector_registers())

    if spec_a.name == "sve-fmla" or spec_b.name == "sve-fmla":
        lines.append("    ptrue p0.s")
    if spec_a.name == "sve-ld1h" or spec_b.name == "sve-ld1h":
        lines.append("    ptrue p1.h")

    used_a = registers.a if layout.body_a else ()
    used_b = registers.b if layout.body_b else ()
    for register in used_a:
        initializer = spec_a.initialize(register)
        if initializer is not None:
            lines.append(f"    {initializer}")
    for register in used_b:
        initializer = spec_b.initialize(register)
        if initializer is not None:
            lines.append(f"    {initializer}")

    lines.extend(("    mov x2, x0", ".Lcpufb_pair_loop:"))
    next_a = 0
    next_b = 0
    for token in layout.tokens:
        if token == "A":
            register = registers.a[next_a % len(registers.a)]
            next_a += 1
            instruction = spec_a.instruction(register)
        else:
            register = registers.b[next_b % len(registers.b)]
            next_b += 1
            instruction = spec_b.instruction(register)
        lines.append(f"    {instruction}")
    lines.extend(("    subs x2, x2, #1", "    b.ne .Lcpufb_pair_loop"))

    if _uses_register_bank(spec_a, spec_b, "vector"):
        lines.extend(f"    {line}" for line in _restore_vector_registers())
    if _uses_register_bank(spec_a, spec_b, "gpr"):
        lines.append("    ldp x19, x20, [sp], #16")
    lines.extend(("    ret", ".size cpufb_pair_probe, .-cpufb_pair_probe", ""))
    return "\n".join(lines)


def generate_driver() -> str:
    return """#include <stdint.h>
#include <stdlib.h>

enum { LOAD_BUFFER_BYTES = 4096 };

static uint8_t load_buffer[LOAD_BUFFER_BYTES]
    __attribute__((aligned(4096)));

extern void cpufb_pair_probe(uint64_t loops, const void *load_base);

int main(int argc, char **argv) {
    uint64_t loops = 1000000;
    if (argc == 2) {
        loops = strtoull(argv[1], NULL, 0);
    }
    if (loops == 0) {
        return 2;
    }
    for (size_t i = 0; i < LOAD_BUFFER_BYTES; ++i) {
        load_buffer[i] = (uint8_t)i;
    }
    cpufb_pair_probe(loops, load_buffer);
    return 0;
}
"""


def parse_perf_stat(text: str) -> CounterSample:
    values: Dict[str, float] = {}
    for raw_line in text.splitlines():
        fields = [field.strip() for field in raw_line.split(",")]
        if len(fields) < 3:
            continue
        event = fields[2].lower()
        raw_value = fields[0].replace(" ", "")
        if not raw_value or raw_value.startswith("<"):
            continue
        try:
            value = float(raw_value)
        except ValueError:
            continue
        if "instructions" in event:
            values["instructions"] = value
        elif "cycles" in event:
            values["cycles"] = value
    missing = [key for key in ("cycles", "instructions") if key not in values]
    if missing:
        raise RuntimeError(
            "perf did not return " + ", ".join(missing) + ":\n" + text.strip()
        )
    return CounterSample(values["cycles"], values["instructions"])


class Arm64PerfRunner:
    def __init__(
        self,
        core: int,
        compiler: str,
        loops: int,
        repetitions: int,
        target_body_instructions: int,
        sudo_perf: bool = False,
        keep_temps: Optional[Path] = None,
    ) -> None:
        self.core = core
        self.compiler = compiler
        self.loops = loops
        self.repetitions = repetitions
        self.target_body_instructions = target_body_instructions
        self.sudo_perf = sudo_perf
        self._temporary: Optional[tempfile.TemporaryDirectory[str]] = None
        if keep_temps is None:
            self._temporary = tempfile.TemporaryDirectory(
                prefix="cpufb-pair-issue-"
            )
            self.directory = Path(self._temporary.name)
        else:
            self.directory = keep_temps
            self.directory.mkdir(parents=True, exist_ok=True)

    def close(self) -> None:
        if self._temporary is not None:
            self._temporary.cleanup()
            self._temporary = None

    def __enter__(self) -> "Arm64PerfRunner":
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()

    def _compile(self, assembly: str, needs_sve: bool) -> Path:
        digest = hashlib.sha256(assembly.encode("utf-8")).hexdigest()[:16]
        source = self.directory / f"probe-{digest}.S"
        driver = self.directory / "driver.c"
        executable = self.directory / f"probe-{digest}"
        if executable.exists():
            return executable
        source.write_text(assembly, encoding="utf-8")
        if not driver.exists():
            driver.write_text(generate_driver(), encoding="utf-8")
        march = "armv8.2-a+sve" if needs_sve else "armv8-a+simd"
        command = [
            self.compiler,
            "-O2",
            f"-march={march}",
            str(driver),
            str(source),
            "-o",
            str(executable),
        ]
        completed = subprocess.run(
            command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        if completed.returncode != 0:
            raise RuntimeError(
                "failed to compile generated pair probe:\n"
                + completed.stderr.strip()
            )
        return executable

    def _run_once(self, executable: Path, loops: int) -> CounterSample:
        perf_command = ["sudo", "-n", "perf"] if self.sudo_perf else ["perf"]
        command = [
            "taskset",
            "-c",
            str(self.core),
            *perf_command,
            "stat",
            "-x,",
            "-e",
            "cycles:u,instructions:u",
            "--",
            str(executable),
            str(loops),
        ]
        environment = dict(os.environ)
        environment["LC_ALL"] = "C"
        completed = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=environment,
        )
        if completed.returncode != 0:
            if completed.returncode in (-4, 132):
                detail = "generated instruction is unsupported (SIGILL)"
            else:
                detail = completed.stderr.strip()
            raise RuntimeError(f"perf probe failed: {detail}")
        return parse_perf_stat(completed.stderr)

    def measure(
        self,
        spec_a: InstructionSpec,
        spec_b: InstructionSpec,
        registers: RegisterAssignment,
        count_a: int,
        count_b: int,
        schedule: str,
        alpha: float,
    ) -> Measurement:
        layout = make_body_layout(
            count_a,
            count_b,
            schedule,
            self.target_body_instructions,
            len(registers.a),
            len(registers.b),
        )
        assembly = generate_assembly(spec_a, spec_b, registers, layout)
        executable = self._compile(
            assembly, spec_a.requires_sve or spec_b.requires_sve
        )
        warmup_loops = max(1000, self.loops // 20)
        warmup = subprocess.run(
            ["taskset", "-c", str(self.core), str(executable), str(warmup_loops)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if warmup.returncode != 0:
            raise RuntimeError(
                "pair probe warm-up failed; the instruction may be unsupported: "
                + warmup.stderr.strip()
            )

        samples = [
            self._run_once(executable, self.loops)
            for _ in range(self.repetitions)
        ]
        median_sample = sorted(samples, key=lambda sample: sample.cycles)[
            len(samples) // 2
        ]
        return Measurement(
            alpha=alpha,
            schedule=schedule,
            base_a=count_a,
            base_b=count_b,
            body_a=layout.body_a,
            body_b=layout.body_b,
            pattern_repetitions=layout.pattern_repetitions,
            cycles_per_loop=median_sample.cycles / self.loops,
            retired_per_loop=median_sample.instructions / self.loops,
            repeat_cycles=[sample.cycles / self.loops for sample in samples],
            repeat_retired=[
                sample.instructions / self.loops for sample in samples
            ],
        )


def measurement_metrics(
    measurement: Measurement, peak_a: float, peak_b: float
) -> Dict[str, Optional[float]]:
    cycles = measurement.cycles_per_loop
    ipc_a = measurement.body_a / cycles
    ipc_b = measurement.body_b / cycles
    serial_cycles = measurement.body_a / peak_a + measurement.body_b / peak_b
    ideal_cycles = max(
        measurement.body_a / peak_a, measurement.body_b / peak_b
    )
    denominator = serial_cycles - ideal_cycles
    overlap = None
    if denominator > 0.0:
        overlap = (serial_cycles - cycles) / denominator
    expected_retired = measurement.body_a + measurement.body_b + 2.0
    return {
        "cycles_per_pattern": cycles / measurement.pattern_repetitions,
        "ipc_a": ipc_a,
        "ipc_b": ipc_b,
        "total_body_ipc": ipc_a + ipc_b,
        "retired_ipc": measurement.retired_per_loop / cycles,
        "efficiency_a": ipc_a / peak_a,
        "efficiency_b": ipc_b / peak_b,
        "serial_cycles": serial_cycles,
        "ideal_cycles": ideal_cycles,
        "overlap": overlap,
        "retired_count_ratio": measurement.retired_per_loop / expected_retired,
    }


def summarize_best_points(
    measurements: Sequence[Measurement], peak_a: float, peak_b: float
) -> List[PointSummary]:
    grouped: Dict[float, List[Measurement]] = {}
    for measurement in measurements:
        grouped.setdefault(measurement.alpha, []).append(measurement)
    points = [PointSummary(0.0, 0.0, 1.0, None)]
    for alpha in sorted(grouped):
        best = min(grouped[alpha], key=lambda item: item.cycles_per_loop)
        metrics = measurement_metrics(best, peak_a, peak_b)
        points.append(
            PointSummary(
                alpha,
                float(metrics["efficiency_a"]),
                float(metrics["efficiency_b"]),
                metrics["overlap"],
            )
        )
    points.append(PointSummary(1.0, 1.0, 0.0, None))
    return points


def _crosses(left: float, right: float, threshold: float) -> bool:
    return (left < threshold <= right) or (right < threshold <= left)


def refinement_candidates(
    points: Sequence[PointSummary],
    tolerance: float,
    protect_threshold: float,
    protect: str,
    minimum_alpha_step: float = 0.02,
) -> List[Tuple[float, float, str]]:
    """Return (priority, alpha, reason), highest priority first."""
    ordered = sorted(points, key=lambda point: point.alpha)
    candidates: Dict[float, Tuple[float, float, str]] = {}

    # Refine a nonlinear overlap frontier. Endpoints have no overlap because
    # serial and ideal times are identical for a pure stream.
    mixed = [point for point in ordered if point.overlap is not None]
    for left, middle, right in zip(mixed, mixed[1:], mixed[2:]):
        span = right.alpha - left.alpha
        if span <= 0.0:
            continue
        fraction = (middle.alpha - left.alpha) / span
        expected = float(left.overlap) + fraction * (
            float(right.overlap) - float(left.overlap)
        )
        deviation = abs(float(middle.overlap) - expected)
        if deviation <= tolerance:
            continue
        for low, high in ((left.alpha, middle.alpha), (middle.alpha, right.alpha)):
            if high - low > minimum_alpha_step:
                alpha = round((low + high) / 2.0, 8)
                candidates[alpha] = (deviation, alpha, "nonlinear-overlap")

    # Locate the largest B pressure that preserves A (and symmetrically B when
    # requested). This is a bisection around the sampled efficiency crossing,
    # not a rectangular ratio grid.
    for left, right in zip(ordered, ordered[1:]):
        if right.alpha - left.alpha <= minimum_alpha_step:
            continue
        reasons: List[str] = []
        priority = 0.0
        if protect in ("a", "both") and _crosses(
            left.efficiency_a, right.efficiency_a, protect_threshold
        ):
            reasons.append("A-knee")
            priority = max(
                priority,
                abs(right.efficiency_a - left.efficiency_a),
            )
        if protect in ("b", "both") and _crosses(
            left.efficiency_b, right.efficiency_b, protect_threshold
        ):
            reasons.append("B-knee")
            priority = max(
                priority,
                abs(right.efficiency_b - left.efficiency_b),
            )
        if reasons:
            alpha = round((left.alpha + right.alpha) / 2.0, 8)
            current = candidates.get(alpha)
            candidate = (priority, alpha, "+".join(reasons))
            if current is None or candidate[0] > current[0]:
                candidates[alpha] = candidate

    return sorted(candidates.values(), key=lambda item: (-item[0], item[1]))


def schedule_sensitivity(
    measurements: Iterable[Measurement], alpha: float
) -> Optional[float]:
    selected = {
        item.schedule: item
        for item in measurements
        if math.isclose(item.alpha, alpha, abs_tol=1e-9)
    }
    if "interleaved" not in selected or "blocked" not in selected:
        return None
    cycles = [
        selected["interleaved"].cycles_per_loop,
        selected["blocked"].cycles_per_loop,
    ]
    return (max(cycles) - min(cycles)) / min(cycles)


def _validate_host(
    spec_a: InstructionSpec, spec_b: InstructionSpec, sudo_perf: bool
) -> None:
    machine = platform.machine().lower()
    if sys.platform != "linux" or machine not in ("aarch64", "arm64"):
        raise RuntimeError("live pair probing requires Linux AArch64")
    for executable in ("taskset", "perf"):
        if shutil.which(executable) is None:
            raise RuntimeError(f"required executable not found: {executable}")
    if sudo_perf and shutil.which("sudo") is None:
        raise RuntimeError("--sudo-perf was requested, but sudo was not found")
    if spec_a.requires_sve or spec_b.requires_sve:
        cpuinfo = Path("/proc/cpuinfo").read_text(
            encoding="utf-8", errors="replace"
        )
        tokens = set(cpuinfo.lower().replace(":", " ").split())
        if "sve" not in tokens:
            raise RuntimeError("the selected pair requires SVE, but SVE is absent")


def _parse_alphas(text: str) -> List[float]:
    values: List[float] = []
    for token in text.split(","):
        try:
            alpha = float(token)
        except ValueError as exc:
            raise argparse.ArgumentTypeError(f"invalid alpha: {token}") from exc
        if not 0.0 < alpha < 1.0:
            raise argparse.ArgumentTypeError("alphas must lie between 0 and 1")
        values.append(alpha)
    if not values:
        raise argparse.ArgumentTypeError("at least one alpha is required")
    return sorted(set(values))


def _pure_measurements(
    runner: Arm64PerfRunner,
    spec_a: InstructionSpec,
    spec_b: InstructionSpec,
    registers: RegisterAssignment,
) -> Tuple[List[Dict[str, object]], float, float]:
    records: List[Dict[str, object]] = []
    peaks: List[float] = []
    for role, registers_for_role in (("A", registers.a), ("B", registers.b)):
        half_size = max(1, len(registers_for_role) // 2)
        role_ipcs: List[float] = []
        for chain_count in (half_size, len(registers_for_role)):
            if role == "A":
                assignment = RegisterAssignment(
                    registers.a[:chain_count], registers.b
                )
                measurement = runner.measure(
                    spec_a, spec_b, assignment, 1, 0, "interleaved", 1.0
                )
                body_count = measurement.body_a
            else:
                assignment = RegisterAssignment(
                    registers.a, registers.b[:chain_count]
                )
                measurement = runner.measure(
                    spec_a, spec_b, assignment, 0, 1, "interleaved", 0.0
                )
                body_count = measurement.body_b
            ipc = body_count / measurement.cycles_per_loop
            role_ipcs.append(ipc)
            records.append(
                {
                    "role": role,
                    "chains": chain_count,
                    "ipc": ipc,
                    "cycles_per_loop": measurement.cycles_per_loop,
                    "repeat_cycles": measurement.repeat_cycles,
                }
            )
        # The full-chain result is the comparable baseline for all mixed runs;
        # the half result is retained as the dependency/unroll stability check.
        peaks.append(role_ipcs[-1])
    return records, peaks[0], peaks[1]


def _measure_alpha(
    runner: Arm64PerfRunner,
    spec_a: InstructionSpec,
    spec_b: InstructionSpec,
    registers: RegisterAssignment,
    peak_a: float,
    peak_b: float,
    alpha: float,
    max_period: int,
    tolerance: float,
) -> List[Measurement]:
    count_a, count_b = normalized_counts(
        peak_a, peak_b, alpha, max_period=max_period
    )
    measurements = [
        runner.measure(
            spec_a,
            spec_b,
            registers,
            count_a,
            count_b,
            schedule,
            alpha,
        )
        for schedule in ("interleaved", "blocked")
    ]
    sensitivity = schedule_sensitivity(measurements, alpha)
    if sensitivity is not None and sensitivity > tolerance:
        measurements.append(
            runner.measure(
                spec_a,
                spec_b,
                registers,
                count_a,
                count_b,
                "shifted",
                alpha,
            )
        )
    return measurements


def run_live(args: argparse.Namespace) -> Dict[str, object]:
    spec_a = INSTRUCTION_SPECS[args.instruction_a]
    spec_b = INSTRUCTION_SPECS[args.instruction_b]
    _validate_host(spec_a, spec_b, args.sudo_perf)
    if shutil.which(args.compiler) is None:
        raise RuntimeError(f"compiler not found: {args.compiler}")
    registers = assign_registers(spec_a, spec_b)
    keep_temps = Path(args.keep_temps) if args.keep_temps else None

    with Arm64PerfRunner(
        args.core,
        args.compiler,
        args.loops,
        args.repetitions,
        args.body_instructions,
        args.sudo_perf,
        keep_temps,
    ) as runner:
        pure, peak_a, peak_b = _pure_measurements(
            runner, spec_a, spec_b, registers
        )
        measurements: List[Measurement] = []
        sampled = set(args.alphas)
        for alpha in sorted(sampled):
            measurements.extend(
                _measure_alpha(
                    runner,
                    spec_a,
                    spec_b,
                    registers,
                    peak_a,
                    peak_b,
                    alpha,
                    args.max_period,
                    args.tolerance,
                )
            )

        refinement_log: List[Dict[str, object]] = []
        while len(sampled) < args.max_points:
            points = summarize_best_points(measurements, peak_a, peak_b)
            candidates = refinement_candidates(
                points,
                args.tolerance,
                args.protect_threshold,
                args.protect,
            )
            candidate = next(
                (item for item in candidates if item[1] not in sampled), None
            )
            if candidate is None:
                break
            priority, alpha, reason = candidate
            sampled.add(alpha)
            refinement_log.append(
                {"alpha": alpha, "reason": reason, "priority": priority}
            )
            measurements.extend(
                _measure_alpha(
                    runner,
                    spec_a,
                    spec_b,
                    registers,
                    peak_a,
                    peak_b,
                    alpha,
                    args.max_period,
                    args.tolerance,
                )
            )

    measurement_records: List[Dict[str, object]] = []
    for measurement in sorted(
        measurements, key=lambda item: (item.alpha, item.schedule)
    ):
        record = asdict(measurement)
        record["metrics"] = measurement_metrics(measurement, peak_a, peak_b)
        record["schedule_sensitivity"] = schedule_sensitivity(
            measurements, measurement.alpha
        )
        measurement_records.append(record)

    warnings: List[str] = []
    for role in ("A", "B"):
        role_records = [item for item in pure if item["role"] == role]
        half_ipc = float(role_records[0]["ipc"])
        full_ipc = float(role_records[-1]["ipc"])
        change = abs(full_ipc - half_ipc) / full_ipc
        if change > args.unroll_tolerance:
            warnings.append(
                f"{role} pure IPC changes {change:.1%} from half to full "
                "accumulator chains; treat the full-chain peak as a lower bound"
            )
    for record in measurement_records:
        ratio = float(record["metrics"]["retired_count_ratio"])
        if abs(ratio - 1.0) > args.retired_tolerance:
            warnings.append(
                f"alpha={float(record['alpha']):.4f} "
                f"schedule={record['schedule']} retired-count ratio is "
                f"{ratio:.3f}; increase --loops before trusting the result"
            )

    return {
        "feature": "experimental-pair-issue",
        "instruction_a": asdict(spec_a),
        "instruction_b": asdict(spec_b),
        "core": args.core,
        "settings": {
            "initial_alphas": args.alphas,
            "max_points": args.max_points,
            "max_period": args.max_period,
            "tolerance": args.tolerance,
            "protect": args.protect,
            "protect_threshold": args.protect_threshold,
            "loops": args.loops,
            "repetitions": args.repetitions,
            "body_instructions": args.body_instructions,
            "sudo_perf": args.sudo_perf,
        },
        "pure": pure,
        "peak_ipc": {"a": peak_a, "b": peak_b},
        "refinements": refinement_log,
        "measurements": measurement_records,
        "warnings": warnings,
    }


def dry_run(args: argparse.Namespace) -> Dict[str, object]:
    if args.peak_a is None or args.peak_b is None:
        raise ValueError("--dry-run requires --peak-a and --peak-b")
    plans = []
    for alpha in args.alphas:
        count_a, count_b = normalized_counts(
            args.peak_a, args.peak_b, alpha, args.max_period
        )
        plans.append(
            {
                "alpha": alpha,
                "count_a": count_a,
                "count_b": count_b,
                "normalized_pressure_a": count_a / args.peak_a,
                "normalized_pressure_b": count_b / args.peak_b,
                "schedules": ["interleaved", "blocked"],
            }
        )
    return {
        "feature": "experimental-pair-issue-dry-run",
        "instruction_a": args.instruction_a,
        "instruction_b": args.instruction_b,
        "peak_ipc": {"a": args.peak_a, "b": args.peak_b},
        "plans": plans,
    }


def _format_optional(value: Optional[float], width: int = 7) -> str:
    if value is None:
        return "-".rjust(width)
    return f"{value:.3f}".rjust(width)


def print_text(report: Dict[str, object]) -> None:
    if report["feature"].endswith("dry-run"):
        peaks = report["peak_ipc"]
        print(
            f"A={report['instruction_a']} peak={peaks['a']:.3f} IPC; "
            f"B={report['instruction_b']} peak={peaks['b']:.3f} IPC"
        )
        print("alpha   A:B    A/Pa    B/Pb  schedules")
        for plan in report["plans"]:
            print(
                f"{plan['alpha']:5.3f}  "
                f"{plan['count_a']:2d}:{plan['count_b']:<2d}  "
                f"{plan['normalized_pressure_a']:6.3f}  "
                f"{plan['normalized_pressure_b']:6.3f}  "
                + ",".join(plan["schedules"])
            )
        return

    spec_a = report["instruction_a"]
    spec_b = report["instruction_b"]
    peaks = report["peak_ipc"]
    print(
        f"Experimental pair issue on CPU {report['core']}: "
        f"A={spec_a['name']}, B={spec_b['name']}"
    )
    print(f"Pure peaks: A={peaks['a']:.3f} IPC, B={peaks['b']:.3f} IPC")
    print(
        "alpha schedule       A:B    cyc/group  "
        "A-IPC  B-IPC total  eff-A  eff-B overlap"
    )
    for record in report["measurements"]:
        metrics = record["metrics"]
        print(
            f"{record['alpha']:5.3f} "
            f"{record['schedule']:<13} "
            f"{record['base_a']:2d}:{record['base_b']:<2d} "
            f"{metrics['cycles_per_pattern']:10.3f} "
            f"{metrics['ipc_a']:6.3f} {metrics['ipc_b']:6.3f} "
            f"{metrics['total_body_ipc']:5.3f} "
            f"{metrics['efficiency_a']:6.3f} "
            f"{metrics['efficiency_b']:6.3f} "
            f"{_format_optional(metrics['overlap'])}"
        )
    if report["refinements"]:
        print("Adaptive refinements:")
        for item in report["refinements"]:
            print(f"  alpha={item['alpha']:.4f}: {item['reason']}")
    if report["warnings"]:
        print("Warnings:")
        for warning in report["warnings"]:
            print(f"  {warning}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "experimentally measure sparse/adaptive joint IPC for two "
            "AArch64 instruction classes"
        )
    )
    parser.add_argument("instruction_a", choices=sorted(INSTRUCTION_SPECS))
    parser.add_argument("instruction_b", choices=sorted(INSTRUCTION_SPECS))
    parser.add_argument("--core", type=int, default=0)
    parser.add_argument("--compiler", default=os.environ.get("CC", "cc"))
    parser.add_argument("--loops", type=int, default=1000000)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--body-instructions", type=int, default=256)
    parser.add_argument("--max-period", type=int, default=64)
    parser.add_argument("--alphas", type=_parse_alphas, default=[0.25, 0.5, 0.75])
    parser.add_argument("--max-points", type=int, default=7)
    parser.add_argument("--tolerance", type=float, default=0.03)
    parser.add_argument("--protect", choices=("a", "b", "both", "none"), default="both")
    parser.add_argument("--protect-threshold", type=float, default=0.97)
    parser.add_argument("--unroll-tolerance", type=float, default=0.02)
    parser.add_argument("--retired-tolerance", type=float, default=0.02)
    parser.add_argument(
        "--sudo-perf",
        action="store_true",
        help="run perf as non-interactive sudo when PMU access is restricted",
    )
    parser.add_argument("--keep-temps", metavar="DIR")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--peak-a", type=float)
    parser.add_argument("--peak-b", type=float)
    parser.add_argument("--format", choices=("text", "json"), default="text")
    return parser


def validate_args(args: argparse.Namespace, parser: argparse.ArgumentParser) -> None:
    if args.instruction_a == args.instruction_b:
        parser.error("instruction_a and instruction_b must be different classes")
    positive_names = (
        "loops",
        "repetitions",
        "body_instructions",
        "max_period",
        "max_points",
    )
    for name in positive_names:
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be positive")
    if args.max_points < len(args.alphas):
        parser.error("--max-points cannot be smaller than the initial alpha count")
    for name in ("tolerance", "unroll_tolerance", "retired_tolerance"):
        if getattr(args, name) < 0.0:
            parser.error(f"--{name.replace('_', '-')} cannot be negative")
    if not 0.0 < args.protect_threshold <= 1.0:
        parser.error("--protect-threshold must be in (0, 1]")
    if args.core < 0:
        parser.error("--core cannot be negative")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    validate_args(args, parser)
    try:
        report = dry_run(args) if args.dry_run else run_live(args)
    except (OSError, RuntimeError, ValueError) as exc:
        parser.exit(2, f"pair_issue_test.py: error: {exc}\n")
    if args.format == "json":
        json.dump(report, sys.stdout, indent=2, sort_keys=True)
        print()
    else:
        print_text(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
