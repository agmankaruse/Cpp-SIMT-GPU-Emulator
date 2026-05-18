#!/usr/bin/env python3
import argparse
import csv
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def default_simulator():
    for directory in [ROOT / "build-ninja", ROOT / "build", ROOT / "build" / "Debug", ROOT / "build" / "Release"]:
        for name in ["simt_gpu.exe", "simt_gpu"]:
            candidate = directory / name
            if candidate.exists():
                return candidate
    return ROOT / "build-ninja" / "simt_gpu"


def parse_stats(output):
    stats = {}
    mapping = {
        "Total cycles": "cycles",
        "Instructions issued": "instructions_issued",
        "IPC": "IPC",
        "Warp occupancy": "occupancy",
        "Branch divergence events": "branch_divergence_events",
        "Branch reconvergence events": "reconvergence_events",
        "Number of coalesced transactions": "global_memory_transactions",
        "Coalescing efficiency": "coalescing_efficiency",
        "L1 hit rate": "L1_hit_rate",
        "Shared memory bank conflicts": "shared_memory_bank_conflicts",
        "Barrier stall cycles": "barrier_stall_cycles",
        "Scheduler idle cycles": "scheduler_idle_cycles",
    }
    for line in output.splitlines():
        if ":" not in line:
            continue
        key, value = [part.strip() for part in line.split(":", 1)]
        if key in mapping:
            stats[mapping[key]] = value.replace("%", "")
    return stats


def main():
    parser = argparse.ArgumentParser(description="Run GPU simulator benchmarks.")
    parser.add_argument("--sim", default=str(default_simulator()))
    parser.add_argument("--benchmarks", default=str(ROOT / "benchmarks"))
    parser.add_argument("--output", default=str(ROOT / "outputs" / "gpu_benchmark_results.csv"))
    args = parser.parse_args()

    programs = sorted(Path(args.benchmarks).glob("*.gpuasm"))
    if not programs:
        print("no GPU benchmarks found", file=sys.stderr)
        return 1

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    timeline_dir = output_path.parent / "timelines"
    timeline_dir.mkdir(parents=True, exist_ok=True)

    rows = []
    for program in programs:
        timeline = timeline_dir / f"{program.stem}_warp_timeline.csv"
        completed = subprocess.run(
            [args.sim, "--timeline", str(timeline), str(program)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        stats = parse_stats(completed.stdout)
        row = {
            "benchmark": program.stem,
            "status": "PASS" if completed.returncode == 0 else "FAIL",
            "cycles": stats.get("cycles", "0"),
            "instructions_issued": stats.get("instructions_issued", "0"),
            "IPC": stats.get("IPC", "0"),
            "occupancy": stats.get("occupancy", "0"),
            "branch_divergence_events": stats.get("branch_divergence_events", "0"),
            "reconvergence_events": stats.get("reconvergence_events", "0"),
            "global_memory_transactions": stats.get("global_memory_transactions", "0"),
            "coalescing_efficiency": stats.get("coalescing_efficiency", "0"),
            "L1_hit_rate": stats.get("L1_hit_rate", "0"),
            "shared_memory_bank_conflicts": stats.get("shared_memory_bank_conflicts", "0"),
            "barrier_stall_cycles": stats.get("barrier_stall_cycles", "0"),
            "scheduler_idle_cycles": stats.get("scheduler_idle_cycles", "0"),
            "timeline_csv": str(timeline.relative_to(ROOT)),
        }
        rows.append(row)
        print(f"{row['status']} {program.stem} cycles={row['cycles']} ipc={row['IPC']}")

    with output_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {output_path}")
    return 0 if all(row["status"] == "PASS" for row in rows) else 1


if __name__ == "__main__":
    sys.exit(main())
