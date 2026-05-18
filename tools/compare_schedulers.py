#!/usr/bin/env python3
import argparse
import csv
import subprocess
from pathlib import Path

from run_gpu_benchmarks import ROOT, default_simulator, parse_stats


def main():
    parser = argparse.ArgumentParser(description="Compare GPU warp scheduling policies.")
    parser.add_argument("benchmark", nargs="?", default=str(ROOT / "benchmarks" / "scheduler_latency_hiding.gpuasm"))
    parser.add_argument("--sim", default=str(default_simulator()))
    parser.add_argument("--output", default=str(ROOT / "outputs" / "scheduler_comparison.csv"))
    args = parser.parse_args()

    policies = ["round_robin", "greedy_then_oldest", "oldest_ready"]
    rows = []
    for policy in policies:
        completed = subprocess.run(
            [args.sim, "--scheduler", policy, args.benchmark],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        stats = parse_stats(completed.stdout)
        rows.append({
            "scheduler": policy,
            "status": "PASS" if completed.returncode == 0 else "FAIL",
            "cycles": stats.get("cycles", "0"),
            "IPC": stats.get("IPC", "0"),
            "issue_efficiency": "see stats",
            "scheduler_idle_cycles": stats.get("scheduler_idle_cycles", "0"),
            "average_eligible_warps": "see stats",
            "memory_stall_cycles": "see stats",
        })
        print(f"{rows[-1]['status']} {policy} cycles={rows[-1]['cycles']} ipc={rows[-1]['IPC']}")

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    report = output.with_name("scheduler_comparison_report.txt")
    report.write_text("\n".join(f"{row['scheduler']}: cycles={row['cycles']} IPC={row['IPC']}" for row in rows) + "\n",
                      encoding="utf-8")
    print(f"wrote {output}")
    print(f"wrote {report}")
    return 0 if all(row["status"] == "PASS" for row in rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
