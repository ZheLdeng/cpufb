#!/usr/bin/env python3
"""Estimate instruction issue rates for a fixed compute/load/store mix.

The default profile models 128-bit SVE FP32 FMLA, LD1W, and ST1W on the
Neoverse V3 machine used for cpufb development.  It is an issue-ceiling model,
not a cache or DRAM performance model.
"""

import argparse
import json
import math
import re
import sys
from dataclasses import dataclass
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple


@dataclass(frozen=True)
class Mix:
    compute: float
    load: float
    store: float


@dataclass(frozen=True)
class Constraint:
    name: str
    capacity: float
    weights: Tuple[float, float, float]
    source: str
    requires_positive: Tuple[str, ...] = ()

    def demand_per_group(self, mix: Mix) -> float:
        return (
            self.weights[0] * mix.compute
            + self.weights[1] * mix.load
            + self.weights[2] * mix.store
        )

    def is_active(self, mix: Mix) -> bool:
        return all(getattr(mix, name) > 0.0 for name in self.requires_positive)


@dataclass(frozen=True)
class Profile:
    name: str
    vector_bytes: float
    ops_per_compute: float
    constraints: Tuple[Constraint, ...]

    def constraint(self, name: str) -> Constraint:
        for constraint in self.constraints:
            if constraint.name == name:
                return constraint
        raise KeyError(name)


NEOVERSE_V3_PROFILE = Profile(
    name="neoverse-v3-sve128-fmla",
    vector_bytes=16.0,
    ops_per_compute=8.0,
    constraints=(
        Constraint("compute", 4.0, (1.0, 0.0, 0.0), "Arm guide + measured"),
        Constraint("load", 3.0, (0.0, 1.0, 0.0), "Arm guide + measured"),
        Constraint(
            "store", 2.0, (0.0, 0.0, 1.0), "Arm guide + measured"
        ),
        Constraint(
            "compute_load",
            5.0,
            (1.0, 1.0, 0.0),
            "measured upper envelope",
        ),
        Constraint(
            "compute_store",
            4.0,
            (1.0, 0.0, 1.0),
            "derived from V/V01 pipelines + measured",
        ),
        Constraint(
            "load_store",
            4.0,
            (0.0, 1.0, 1.0),
            "derived from L/SA pipelines + measured",
        ),
        Constraint(
            "mop_dispatch",
            10.0,
            (1.0, 1.0, 1.0),
            "Arm guide",
        ),
        Constraint(
            "uop_dispatch",
            20.0,
            (1.0, 1.0, 2.0),
            "Arm guide; an SVE store is address + data",
        ),
    ),
)

NEOVERSE_V1_FMLA_PROFILE = Profile(
    name="neoverse-v1-sve256-fmla",
    vector_bytes=32.0,
    ops_per_compute=16.0,
    constraints=(
        Constraint(
            "compute",
            1.84,
            (1.0, 0.0, 0.0),
            "AmazonECS8Cores measured sustained ceiling; Arm guide peak is 2",
        ),
        Constraint(
            "load", 2.0, (0.0, 1.0, 0.0), "Arm guide + measured"
        ),
        Constraint(
            "store",
            1.0,
            (0.0, 0.0, 1.0),
            "AmazonECS8Cores measured sustained SVE256 ceiling",
        ),
        Constraint(
            "compute_load",
            3.6,
            (1.0, 1.0, 0.0),
            "measured upper envelope",
        ),
        Constraint(
            "compute_store",
            1.84,
            (1.0, 0.0, 1.0),
            "measured shared vector/store-data ceiling",
        ),
        Constraint(
            "load_store",
            2.0,
            (0.0, 1.0, 1.0),
            "measured shared load/store issue ceiling",
        ),
        Constraint(
            "mop_dispatch",
            8.0,
            (1.0, 1.0, 1.0),
            "Arm guide",
        ),
        Constraint(
            "uop_dispatch",
            16.0,
            (1.0, 1.0, 2.0),
            "Arm guide; a store is modeled as address + data",
        ),
    ),
)

NEOVERSE_V1_BFMMLA_PROFILE = Profile(
    name="neoverse-v1-sve256-bfmmla",
    vector_bytes=32.0,
    ops_per_compute=64.0,
    constraints=(
        Constraint(
            "compute",
            2.0,
            (1.0, 0.0, 0.0),
            "Arm guide + AmazonECS8Cores measured",
        ),
        Constraint(
            "load", 2.0, (0.0, 1.0, 0.0), "Arm guide + measured"
        ),
        Constraint(
            "store",
            1.0,
            (0.0, 0.0, 1.0),
            "AmazonECS8Cores measured sustained SVE256 ceiling",
        ),
        Constraint(
            "compute_load",
            3.67,
            (1.0, 1.0, 0.0),
            "measured balanced BFMMLA + load upper envelope",
        ),
        Constraint(
            "compute_store",
            2.0,
            (1.0, 0.0, 1.0),
            "measured shared BFMMLA/store-data ceiling",
        ),
        Constraint(
            "load_store",
            2.0,
            (0.0, 1.0, 1.0),
            "measured shared load/store issue ceiling",
        ),
        Constraint(
            "compute_when_loading",
            1.84,
            (1.0, 0.0, 0.0),
            "measured BFMMLA ceiling when load traffic is present",
            ("load",),
        ),
        Constraint(
            "compute_store_when_loading",
            1.86,
            (1.0, 0.0, 1.0),
            "measured BFMMLA + store-data ceiling with load traffic",
            ("load",),
        ),
        Constraint(
            "mop_dispatch",
            8.0,
            (1.0, 1.0, 1.0),
            "Arm guide",
        ),
        Constraint(
            "uop_dispatch",
            16.0,
            (1.0, 1.0, 2.0),
            "Arm guide; a store is modeled as address + data",
        ),
    ),
)

PROFILES = {
    profile.name: profile
    for profile in (
        NEOVERSE_V3_PROFILE,
        NEOVERSE_V1_FMLA_PROFILE,
        NEOVERSE_V1_BFMMLA_PROFILE,
    )
}
DEFAULT_PROFILE = NEOVERSE_V3_PROFILE


MIX_TOKEN = re.compile(
    r"(?P<count>(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))(?P<kind>[FfCcLlSs])"
)


def parse_mix(value: str) -> Mix:
    compact = re.sub(r"\s+", "", value)
    if not compact:
        raise ValueError("mix must not be empty")

    tokens = re.split(r"[+,/:]", compact)
    values = {"compute": 0.0, "load": 0.0, "store": 0.0}
    seen = set()
    for token in tokens:
        match = MIX_TOKEN.fullmatch(token)
        if match is None:
            raise ValueError(
                "invalid mix token {!r}; use forms such as 4F+5L+1S".format(token)
            )
        kind = match.group("kind").upper()
        field = {"F": "compute", "C": "compute", "L": "load", "S": "store"}[kind]
        if field in seen:
            raise ValueError("mix contains {} more than once".format(field))
        seen.add(field)
        count = float(match.group("count"))
        if not math.isfinite(count) or count < 0.0:
            raise ValueError("{} count must be finite and non-negative".format(field))
        values[field] = count

    mix = Mix(**values)
    if mix.compute + mix.load + mix.store <= 0.0:
        raise ValueError("mix must contain at least one instruction")
    return mix


def parse_overrides(
    values: Iterable[str], profile: Profile = DEFAULT_PROFILE
) -> Dict[str, float]:
    valid_names = {constraint.name for constraint in profile.constraints}
    overrides: Dict[str, float] = {}
    for value in values:
        if "=" not in value:
            raise ValueError(
                "invalid constraint override {!r}; use NAME=CAPACITY".format(value)
            )
        name, raw_capacity = value.split("=", 1)
        name = name.strip()
        if name not in valid_names:
            raise ValueError(
                "unknown constraint {!r}; choose from {}".format(
                    name, ", ".join(sorted(valid_names))
                )
            )
        try:
            capacity = float(raw_capacity)
        except ValueError as error:
            raise ValueError(
                "invalid capacity for {}: {!r}".format(name, raw_capacity)
            ) from error
        if not math.isfinite(capacity) or capacity <= 0.0:
            raise ValueError("{} capacity must be finite and positive".format(name))
        overrides[name] = capacity
    return overrides


def with_overrides(profile: Profile, overrides: Mapping[str, float]) -> Profile:
    constraints = tuple(
        Constraint(
            constraint.name,
            overrides.get(constraint.name, constraint.capacity),
            constraint.weights,
            (
                constraint.source
                if constraint.name not in overrides
                else constraint.source + " + CLI override"
            ),
            constraint.requires_positive,
        )
        for constraint in profile.constraints
    )
    return Profile(
        name=profile.name,
        vector_bytes=profile.vector_bytes,
        ops_per_compute=profile.ops_per_compute,
        constraints=constraints,
    )


def _component_rates(mix: Mix, cycles_per_group: float) -> Dict[str, float]:
    return {
        "compute": mix.compute / cycles_per_group,
        "load": mix.load / cycles_per_group,
        "store": mix.store / cycles_per_group,
    }


def evaluate(
    mix: Mix,
    profile: Profile = DEFAULT_PROFILE,
    observed_cycles_per_group: Optional[float] = None,
    vector_bytes: Optional[float] = None,
    ops_per_compute: Optional[float] = None,
) -> Dict[str, object]:
    contributions: Dict[str, float] = {}
    for constraint in profile.constraints:
        contributions[constraint.name] = (
            constraint.demand_per_group(mix) / constraint.capacity
            if constraint.is_active(mix)
            else 0.0
        )

    predicted_cycles = max(contributions.values())
    if predicted_cycles <= 0.0:
        raise ValueError("the selected profile produced a zero cycle estimate")

    if observed_cycles_per_group is not None:
        if (
            not math.isfinite(observed_cycles_per_group)
            or observed_cycles_per_group <= 0.0
        ):
            raise ValueError("observed cycles/group must be finite and positive")
        result_cycles = observed_cycles_per_group
        mode = "observed"
    else:
        result_cycles = predicted_cycles
        mode = "predicted_upper_bound"

    rates = _component_rates(mix, result_cycles)
    total_ipc = rates["compute"] + rates["load"] + rates["store"]
    constraint_utilization = {
        name: contribution / result_cycles
        for name, contribution in contributions.items()
    }
    predicted_constraint_utilization = {
        name: contribution / predicted_cycles
        for name, contribution in contributions.items()
    }
    bottlenecks = sorted(
        name
        for name, utilization in predicted_constraint_utilization.items()
        if math.isclose(utilization, 1.0, rel_tol=1e-9, abs_tol=1e-12)
    )

    compute_efficiency = rates["compute"] / profile.constraint("compute").capacity
    load_efficiency = rates["load"] / profile.constraint("load").capacity
    store_efficiency = rates["store"] / profile.constraint("store").capacity
    load_store_efficiency = (
        rates["load"] + rates["store"]
    ) / profile.constraint("load_store").capacity
    memory_efficiency = max(
        load_efficiency, store_efficiency, load_store_efficiency
    )

    selected_vector_bytes = (
        profile.vector_bytes if vector_bytes is None else vector_bytes
    )
    selected_ops_per_compute = (
        profile.ops_per_compute
        if ops_per_compute is None
        else ops_per_compute
    )
    for name, selected_value in (
        ("vector bytes", selected_vector_bytes),
        ("operations per compute instruction", selected_ops_per_compute),
    ):
        if not math.isfinite(selected_value) or selected_value <= 0.0:
            raise ValueError("{} must be finite and positive".format(name))

    shares = {
        "compute": rates["compute"] / total_ipc,
        "memory": (rates["load"] + rates["store"]) / total_ipc,
    }

    return {
        "profile": profile.name,
        "mode": mode,
        "mix": {
            "compute": mix.compute,
            "load": mix.load,
            "store": mix.store,
        },
        "predicted": {
            "cycles_per_group": predicted_cycles,
            "groups_per_cycle": 1.0 / predicted_cycles,
            "bottlenecks": bottlenecks,
            "constraint_utilization": predicted_constraint_utilization,
        },
        "result": {
            "cycles_per_group": result_cycles,
            "groups_per_cycle": 1.0 / result_cycles,
            "compute_ipc": rates["compute"],
            "load_ipc": rates["load"],
            "store_ipc": rates["store"],
            "memory_ipc": rates["load"] + rates["store"],
            "total_ipc": total_ipc,
        },
        "efficiency": {
            "compute": compute_efficiency,
            "load": load_efficiency,
            "store": store_efficiency,
            "load_store_shared": load_store_efficiency,
            "memory": memory_efficiency,
            "model_attainment": predicted_cycles / result_cycles,
        },
        "constraint_utilization": constraint_utilization,
        "shares": shares,
        "traffic": {
            "ops_per_cycle": rates["compute"] * selected_ops_per_compute,
            "read_bytes_per_cycle": rates["load"] * selected_vector_bytes,
            "write_bytes_per_cycle": rates["store"] * selected_vector_bytes,
            "total_bytes_per_cycle": (
                rates["load"] + rates["store"]
            )
            * selected_vector_bytes,
            "vector_bytes": selected_vector_bytes,
            "ops_per_compute": selected_ops_per_compute,
        },
        "constraints": {
            constraint.name: {
                "capacity": constraint.capacity,
                "weights": {
                    "compute": constraint.weights[0],
                    "load": constraint.weights[1],
                    "store": constraint.weights[2],
                },
                "source": constraint.source,
                "requires_positive": list(constraint.requires_positive),
            }
            for constraint in profile.constraints
        },
    }


def _format_count(value: float) -> str:
    if value.is_integer():
        return str(int(value))
    return "{:.6g}".format(value)


def _format_efficiency(value: float, active: bool = True) -> str:
    return "{:8.2f}%".format(value * 100.0) if active else "       -"


def format_text(report: Mapping[str, object]) -> str:
    mix = report["mix"]
    predicted = report["predicted"]
    result = report["result"]
    efficiency = report["efficiency"]
    traffic = report["traffic"]
    constraints = report["constraints"]
    utilization = report["constraint_utilization"]

    mix_text = "{}F + {}L + {}S".format(
        _format_count(mix["compute"]),
        _format_count(mix["load"]),
        _format_count(mix["store"]),
    )
    lines = [
        "Profile: {}".format(report["profile"]),
        "Mix: {} per group".format(mix_text),
        "Mode: {}".format(report["mode"]),
        "Predicted cycles/group: {:.6f}".format(predicted["cycles_per_group"]),
        "Result cycles/group:    {:.6f}".format(result["cycles_per_group"]),
        "Binding constraint(s): {}".format(
            ", ".join(predicted["bottlenecks"])
        ),
        "",
        "Component          IPC       Peak IPC   Efficiency",
        "compute       {:9.4f}      {:7.3f}   {}".format(
            result["compute_ipc"],
            constraints["compute"]["capacity"],
            _format_efficiency(efficiency["compute"], mix["compute"] > 0.0),
        ),
        "load          {:9.4f}      {:7.3f}   {}".format(
            result["load_ipc"],
            constraints["load"]["capacity"],
            _format_efficiency(efficiency["load"], mix["load"] > 0.0),
        ),
        "store         {:9.4f}      {:7.3f}   {}".format(
            result["store_ipc"],
            constraints["store"]["capacity"],
            _format_efficiency(efficiency["store"], mix["store"] > 0.0),
        ),
        "memory        {:9.4f}          n/a   {}".format(
            result["memory_ipc"],
            _format_efficiency(
                efficiency["memory"], mix["load"] + mix["store"] > 0.0
            ),
        ),
        "total         {:9.4f}".format(result["total_ipc"]),
        "",
        "Compute share: {:6.2f}%".format(report["shares"]["compute"] * 100.0),
        "Memory share:  {:6.2f}%".format(report["shares"]["memory"] * 100.0),
        "Compute work:  {:.4f} ops/cycle".format(traffic["ops_per_cycle"]),
        "Read traffic:  {:.4f} B/cycle".format(traffic["read_bytes_per_cycle"]),
        "Write traffic: {:.4f} B/cycle".format(
            traffic["write_bytes_per_cycle"]
        ),
        "",
        "Resource utilization:",
    ]
    resource_name_width = max(15, max(len(name) for name in constraints))
    for name in constraints:
        lines.append(
            "  {:{width}s} {:7.2f}%".format(
                name,
                utilization[name] * 100.0,
                width=resource_name_width,
            )
        )

    if report["mode"] == "observed":
        lines.extend(
            [
                "",
                "Model attainment: {:.2f}%".format(
                    efficiency["model_attainment"] * 100.0
                ),
            ]
        )
    return "\n".join(lines)


def _positive_float(value: str) -> float:
    try:
        parsed = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected a number") from error
    if not math.isfinite(parsed) or parsed <= 0.0:
        raise argparse.ArgumentTypeError("value must be finite and positive")
    return parsed


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Estimate per-component IPC for a compute/load/store instruction mix."
        )
    )
    parser.add_argument(
        "mix",
        help="instructions per logical group, for example 4F+5L or 4F+3L+1S",
    )
    parser.add_argument(
        "--profile",
        choices=sorted(PROFILES),
        default=DEFAULT_PROFILE.name,
        help="machine/instruction profile (default: %(default)s)",
    )
    parser.add_argument(
        "--observed-cycles-per-group",
        type=_positive_float,
        help="use measured cycles/group for result IPC and report model attainment",
    )
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="CONSTRAINT=CAPACITY",
        help="override a profile capacity; may be specified more than once",
    )
    parser.add_argument(
        "--vector-bytes",
        type=_positive_float,
        help="bytes transferred by each load/store instruction (default: profile value)",
    )
    parser.add_argument(
        "--ops-per-compute",
        type=_positive_float,
        help="operations performed by each compute instruction (default: profile value)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="emit machine-readable JSON",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_argument_parser()
    args = parser.parse_args(argv)
    try:
        mix = parse_mix(args.mix)
        base_profile = PROFILES[args.profile]
        overrides = parse_overrides(args.set, base_profile)
        profile = with_overrides(base_profile, overrides)
        report = evaluate(
            mix,
            profile,
            observed_cycles_per_group=args.observed_cycles_per_group,
            vector_bytes=args.vector_bytes,
            ops_per_compute=args.ops_per_compute,
        )
    except ValueError as error:
        parser.error(str(error))

    if args.json:
        json.dump(report, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
    else:
        print(format_text(report))
    return 0


if __name__ == "__main__":
    sys.exit(main())
